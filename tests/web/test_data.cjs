const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

const source = readFileSync(path.join(__dirname, '../../src/shell/web/data.js'), 'utf8');
const data = vm.runInNewContext(source + '\nGameData;', { Blob, Uint8Array, Error });

function file(name, content, relative = '') {
  const blob = new Blob([content]);
  Object.defineProperties(blob, {
    name: { value: name },
    webkitRelativePath: { value: relative },
  });
  return blob;
}

async function stage(selection) {
  const writes = new Map();
  const directories = [];
  const progress = [];
  const FS = {
    mkdirTree: (name) => directories.push(name),
    writeFile: (name, bytes, options) => {
      assert.equal(options.canOwn, true);
      writes.set(name, bytes);
    },
  };
  const root = await data.stage(FS, selection, async (blob, report) => {
    report(blob.size);
    return new Uint8Array(await blob.arrayBuffer());
  }, (done, total) => progress.push([done, total]));
  return { root, writes, directories, progress };
}

test('folders strip only the selected root and preserve nested paths', async () => {
  const selection = data.fromFolder([
    file('th07.dat', 'archive', '妖々夢/th07.dat'),
    file('thbgm.dat', 'music', '妖々夢/thbgm.dat'),
    file('font.ttf', 'font', '妖々夢/opentouhou/fonts/font.ttf'),
  ]);
  const result = await stage(selection);
  assert.equal(result.root, '/game-data');
  assert.deepEqual([...result.writes.keys()], [
    '/game-data/th07.dat', '/game-data/thbgm.dat', '/game-data/opentouhou/fonts/font.ttf',
  ]);
  assert.equal(new TextDecoder().decode(result.writes.get('/game-data/thbgm.dat')), 'music');
  assert.deepEqual(result.progress.at(-1), [16, 16]);
  assert.ok(result.directories.includes('/game-data/opentouhou/fonts'));
});

test('folder selections survive structured cloning for IndexedDB', async () => {
  const selection = data.fromFolder([file('TOUHOU.DAT', 'original', 'th01/TOUHOU.DAT')]);
  const result = await stage(structuredClone(selection));
  assert.equal(result.root, '/game-data');
  assert.equal(new TextDecoder().decode(result.writes.get('/game-data/TOUHOU.DAT')), 'original');
});

test('ZIP files still stage for every game', async () => {
  for (const game of ['th01', 'th06', 'th07']) {
    const selection = data.fromFile(file('Game.ZIP', 'zip'), game);
    const result = await stage(selection);
    assert.equal(result.root, '/game.zip');
    assert.equal(new TextDecoder().decode(result.writes.get('/game.zip')), 'zip');
  }
});

test('th01 accepts bare HDI files and ZIP input', async () => {
  for (const name of ['Game.HDI', 'disk.zip']) {
    const selection = data.fromFile(file(name, 'disk image'), 'th01');
    const result = await stage(selection);
    assert.equal(result.root, name.endsWith('HDI') ? '/game.hdi' : '/game.zip');
  }
  assert.throws(() => data.fromFile(file('game.hdi', ''), 'th07'), /ZIP/);
});

test('remembered ZIP blobs from the old player still stage', async () => {
  const result = await stage(new Blob(['remembered zip']));
  assert.equal(result.root, '/game.zip');
  assert.equal(new TextDecoder().decode(result.writes.get('/game.zip')), 'remembered zip');
});

test('invalid folder paths and duplicate names fail before staging', async () => {
  for (const relative of ['game/../escape', '/absolute', 'game/a\\b', 'game/a\0b', 'game//file']) {
    assert.throws(() => data.fromFolder([file('file', '', relative)]), /path/);
  }
  assert.throws(() => data.fromFolder([]), /empty/);
  assert.throws(() => data.fromFolder([
    file('a', '', 'game/a'), file('a', '', 'game/a'),
  ]), /duplicate/);
  assert.throws(() => data.fromFolder([
    file('a', '', 'first/a'), file('b', '', 'second/b'),
  ]), /one game folder/);
  await assert.rejects(stage({ kind: 'folder', files: [{ path: '../user', file: new Blob() }] }), /path/);
});

function entry(name, content) {
  return { name, isFile: true, file: (resolve) => resolve(file(name, content)) };
}

function directory(name, batches) {
  return {
    name,
    isDirectory: true,
    createReader: () => {
      let index = 0;
      return { readEntries: (resolve) => resolve(batches[index++] || []) };
    },
  };
}

test('dropped folders read all directory batches and preserve subfolders', async () => {
  const selection = await data.fromDirectory(directory('game', [
    [entry('th06.dat', 'data')],
    [directory('bgm', [[entry('track.wav', 'music')]])],
  ]));
  const result = await stage(selection);
  assert.deepEqual([...result.writes.keys()], ['/game-data/th06.dat', '/game-data/bgm/track.wav']);
});

test('read errors propagate and prevent writing the failed file', async () => {
  let writes = 0;
  await assert.rejects(data.stage({ mkdirTree() {}, writeFile() { writes++; } },
    new Blob(['zip']), async () => { throw new Error('cannot read'); }, () => {}), /cannot read/);
  assert.equal(writes, 0);
});
