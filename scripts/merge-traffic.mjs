// Fold a fresh traffic window into the permanent record.
//
// GitHub's traffic API only serves the last 14 days, so anything not captured
// before it ages out is gone for good. This keeps a running history instead.
//
//   node scripts/merge-traffic.mjs prev.json clones.json views.json releases.txt
//
// Writes the merged record to stdout.

import { readFileSync } from "node:fs";

const [prevPath, clonesPath, viewsPath, releasesPath] = process.argv.slice(2);

const read = (path, fallback) => {
  try {
    return JSON.parse(readFileSync(path, "utf8"));
  } catch (e) {
    if (fallback !== undefined) return fallback;
    throw new Error(`${path}: ${e.message}`);
  }
};

const prev = read(prevPath, {});
const clones = read(clonesPath);
const views = read(viewsPath);
const releases = read(releasesPath, 0);

/*
 * The merge is keyed by date and overwrites, which makes it idempotent and
 * self-correcting: the entry for today is partial when first written and is
 * simply replaced by the final figure on a later run. Re-running the workflow
 * any number of times in a day cannot double-count.
 */
function fold(previousDays, fresh) {
  const days = { ...(previousDays || {}) };
  for (const entry of fresh || []) {
    days[entry.timestamp.slice(0, 10)] = {
      count: entry.count,
      uniques: entry.uniques,
    };
  }
  // Keep the file readable and diffs small: oldest day first.
  return Object.fromEntries(Object.entries(days).sort(([a], [b]) => a.localeCompare(b)));
}

/*
 * `uniques` is summed per day, so one person cloning on three separate days
 * counts three times. GitHub only dedupes within a single 14-day window, and
 * once the record outlives that window a true headcount is unrecoverable. The
 * total is an upper bound on people, and the UI labels it as one.
 *
 * `baseline` covers traffic from before collection started, which GitHub has
 * already discarded. It is carried through untouched — a manually supplied
 * figure that no API can confirm, kept as its own field so the measured days
 * stay auditable instead of being silently inflated.
 */
function totals(days, baseline) {
  const sum = key => Object.values(days).reduce((n, d) => n + (d[key] || 0), 0);
  const base = baseline || {};
  return {
    total: (base.count || 0) + sum("count"),
    uniques: (base.uniques || 0) + sum("uniques"),
    measured: sum("count"),
    baseline: base.count || 0,
    days,
  };
}

const merged = {
  updated: new Date().toISOString().replace(/\.\d+Z$/, "Z"),
  clones: totals(fold(prev.clones?.days, clones.clones), prev.baseline?.clones),
  views: totals(fold(prev.views?.days, views.views), prev.baseline?.views),
  releaseDownloads: typeof releases === "number" ? releases : 0,
};

// Preserved verbatim so a later run never recomputes or drops it.
if (prev.baseline) merged.baseline = prev.baseline;

process.stdout.write(JSON.stringify(merged, null, 2) + "\n");
