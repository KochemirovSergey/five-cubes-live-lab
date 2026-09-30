import { readFileSync } from 'node:fs';
export const targetCatalog = JSON.parse(readFileSync(new URL('../unreal/FiveCubes/Content/Lab/targets.json', import.meta.url), 'utf8')).targets;
export const TARGETS = targetCatalog.map(target => target.id);
