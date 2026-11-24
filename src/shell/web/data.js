const GameData = (() => {
  function checkPath(path) {
    if (typeof path !== 'string' || path.includes('\\') || path.includes('\0') ||
        path.split('/').some((part) => !part || part === '.' || part === '..')) {
      throw new Error('Invalid game data path: ' + path);
    }
    return path;
  }

  function folder(entries) {
    if (!entries.length) {
      throw new Error('The selected folder is empty.');
    }
    const paths = new Set();
    for (const { path, file } of entries) {
      checkPath(path);
      if (!(file instanceof Blob) || paths.has(path)) {
        throw new Error('Invalid or duplicate game data file: ' + path);
      }
      paths.add(path);
    }
    return { kind: 'folder', files: entries };
  }

  function fromFolder(files) {
    let root = null;
    return folder(Array.from(files, (file) => {
      const relative = checkPath(file.webkitRelativePath);
      const slash = relative.indexOf('/');
      if (slash < 0) {
        throw new Error('The selected file has no folder path.');
      }
      const parent = relative.slice(0, slash);
      root ??= parent;
      if (root !== parent) {
        throw new Error('Select one game folder.');
      }
      return { path: relative.slice(slash + 1), file };
    }));
  }

  function fromFile(file, game) {
    const allowed = game === 'th01' ? /\.(zip|hdi)$/i : /\.zip$/i;
    if (!file || !allowed.test(file.name)) {
      throw new Error(game === 'th01' ? 'Choose a ZIP or HDI file.' : 'Choose a ZIP file.');
    }
    return file;
  }

  async function fromDirectory(entry) {
    const files = [];
    async function visit(item, path) {
      if (item.isFile) {
        const file = await new Promise((resolve, reject) => item.file(resolve, reject));
        files.push({ path, file });
      } else if (item.isDirectory) {
        const reader = item.createReader();
        for (;;) {
          const children = await new Promise((resolve, reject) => reader.readEntries(resolve, reject));
          if (!children.length) {
            break;
          }
          for (const child of children) {
            await visit(child, path ? path + '/' + child.name : child.name);
          }
        }
      }
    }
    await visit(entry, '');
    return folder(files);
  }

  async function stage(FS, selection, read, report) {
    const directory = selection?.kind === 'folder';
    const files = directory ? folder(selection.files).files : [{
      path: /\.hdi$/i.test(selection?.name) ? 'game.hdi' : 'game.zip',
      file: selection,
    }];
    if (files.some(({ file }) => !(file instanceof Blob))) {
      throw new Error('The saved game data is invalid. Choose it again.');
    }
    const root = directory ? '/game-data/' : '/';
    const total = files.reduce((sum, { file }) => sum + file.size, 0);
    let done = 0;
    report(0, total);
    for (const { path, file } of files) {
      const data = await read(file, (count) => report(done + count, total));
      const full = root + path;
      FS.mkdirTree(full.slice(0, full.lastIndexOf('/')) || '/');
      FS.writeFile(full, data, { canOwn: true });
      done += file.size;
      report(done, total);
    }
    return directory ? '/game-data' : root + files[0].path;
  }

  return { fromFolder, fromFile, fromDirectory, stage };
})();
