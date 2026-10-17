// Snapshot the exact files being confirmed. SSE updates, filter changes, and
// further selections must not change the request while a dialog is open.
export const prepareDownloadDeletion = (hashes, getFile) => prepareDeletion(hashes, getFile, false);
export const prepareSharedDeletion = (hashes, getFile) => prepareDeletion(hashes, getFile, true);

async function prepareDeletion(hashes, getFile, shared) {
  const ids = [...new Set(hashes)];
  const files = [];
  // Bound concurrent detail requests; an arbitrary selection can contain 500 files.
  for (let i = 0; i < ids.length; i += 10) {
    const batch = await Promise.all(ids.slice(i, i + 10).map((h) => getFile(h)));
    batch.forEach((d, j) => files.push({ hash: ids[i + j], ecid: d.ecid,
      name: d.name, directory: d.directory, ...(shared ? { incomplete: d.incomplete } : { status: d.status }) }));
  }
  const confirmed_target = Object.fromEntries(files.map(({ hash, ...target }) => [hash, target]));
  return { hashes: ids, files, confirmed_target };
}

// Remove successful operations from the *current* selection. Failed files and
// files selected after confirmation remain selected for retry/the next action.
export function selectionAfterDeletion(selection, hashes, results) {
  const next = new Set(selection);
  const succeeded = results ? results.filter((r) => r.ok).map((r) => r.id) : hashes;
  succeeded.forEach((h) => next.delete(h));
  return next;
}
