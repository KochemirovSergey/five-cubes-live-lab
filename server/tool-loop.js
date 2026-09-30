// GPT-Live uses its own event protocol, not the Realtime event names.
export class ToolLoop {
  constructor({ execute, send, revision }) {
    this.execute = execute; this.send = send; this.revision = revision;
    this.responses = new Map(); this.seen = new Set(); this.closed = false;
  }
  async event(envelope) {
    if (this.closed || envelope.type !== 'response.event') return;
    const e = envelope.event;
    const id = e.response?.id || e.response_id || envelope.delegation_id;
    if (e.type === 'response.created') {
      this.responses.set(envelope.delegation_id, { id, revision: this.revision(), calls: [] });
    } else if (e.type === 'response.output_item.done' && e.item?.type === 'function_call') {
      let batch = this.responses.get(envelope.delegation_id);
      if (!batch) { batch = { id, revision: this.revision(), calls: [] }; this.responses.set(envelope.delegation_id, batch); }
      const item = e.item;
      if (this.seen.has(item.call_id)) return;
      this.seen.add(item.call_id);
      batch.calls.push(Promise.resolve().then(() => this.execute(item.name, JSON.parse(item.arguments), batch.revision, item.call_id))
        .catch(() => ({ isError: true, content: [{ type: 'text', text: 'TOOL_EXECUTION_FAILED' }] }))
        .then(output => ({ call_id: item.call_id, output })));
    } else if (e.type === 'response.completed') {
      const batch = this.responses.get(envelope.delegation_id);
      this.responses.delete(envelope.delegation_id);
      if (!batch?.calls.length) return;
      const results = await Promise.all(batch.calls);
      if (this.closed) return;
      for (const item of results) this.send({ type: 'response.item.create', event_id: crypto.randomUUID(),
        item: { type: 'function_call_output', call_id: item.call_id, output: JSON.stringify(item.output) } });
      this.send({ type: 'response.create', event_id: crypto.randomUUID() });
    }
  }
  close() { this.closed = true; this.responses.clear(); }
}
