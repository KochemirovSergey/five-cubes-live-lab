import { readFileSync } from 'node:fs';
const read = name => JSON.parse(readFileSync(new URL(`../unreal/FiveCubes/Content/Lab/${name}.json`, import.meta.url), 'utf8'));
export const laboratories = {
  training_panel: { scenario: read('scenario'), targets: read('targets').targets },
  oberbeck: { scenario: read('oberbeck-scenario'), targets: read('oberbeck-targets').targets },
  menu: { scenario: { title: 'Выбор лабораторной', steps: [{ id: 'select', goal: 'Выберите лабораторную' }] }, targets: [] },
};
export const oberbeckProfile = read('oberbeck-physics');
export const fullOberbeck = { scenario: read('oberbeck-full-scenario'), targets: read('oberbeck-full-targets').targets };
export const laboratory = (id, mode = 'intro') => id === 'oberbeck' && mode === 'full' ? fullOberbeck : laboratories[id];
