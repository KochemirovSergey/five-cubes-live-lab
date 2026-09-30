import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { StreamableHTTPServerTransport } from '@modelcontextprotocol/sdk/server/streamableHttp.js';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StreamableHTTPClientTransport } from '@modelcontextprotocol/sdk/client/streamableHttp.js';
import { TARGETS } from './targets.js';
import { z } from 'zod';

export const toolSchemas = {
  lab_get_state: { description: 'Read the current scene and allowed panel IDs.', properties: {} },
  lab_highlight: { description: 'Highlight a panel control; success requires acknowledgment from the scene.',
    properties: { target_id: { type: 'string', enum: TARGETS },
      text: { type: 'string', maxLength: 120 } } },
  lab_clear_highlight: { description: 'Clear the scene highlight.', properties: {} },
};
export const liveTools = Object.entries(toolSchemas).map(([name, spec]) => ({
  type: 'function', name, description: spec.description, strict: true,
  parameters: { type: 'object', properties: spec.properties, required: Object.keys(spec.properties), additionalProperties: false },
}));

export async function handleMcp(req, res, lab) {
  const revision = req.headers['x-lab-revision'] === undefined ? lab.revision : Number(req.headers['x-lab-revision']);
  const server = new McpServer({ name: 'five-cubes', version: '0.1.0' });
  for (const [name, spec] of Object.entries(toolSchemas)) {
    server.registerTool(name, { description: spec.description,
      inputSchema: name === 'lab_highlight' ? { target_id: z.string(), text: z.string().max(120) } : {} },
    args => lab.call(name, args, revision));
  }
  const transport = new StreamableHTTPServerTransport({ sessionIdGenerator: undefined, enableJsonResponse: true });
  res.on('close', () => { void transport.close(); void server.close(); });
  await server.connect(transport);
  await transport.handleRequest(req, res, req.body);
}

// The voice bridge and manual controls use the real HTTP MCP transport too.
export async function callMcp(baseUrl, lab, name, args, revision) {
  const client = new Client({ name: 'lab-voice-bridge', version: '0.1.0' });
  try {
    await client.connect(new StreamableHTTPClientTransport(new URL('/mcp', baseUrl), {
      requestInit: { headers: { Authorization: `Bearer ${lab.token}`, 'x-lab-session': lab.id, 'x-lab-revision': String(revision) } },
    }));
    return await client.callTool({ name, arguments: args });
  } finally { await client.close(); }
}
