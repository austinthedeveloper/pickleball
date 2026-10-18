#ifndef TUNING_H
#define TUNING_H

/*
 * The gameplay tuning version (ADR-068, docs/TUNING.md). Ports compare it
 * with their own to know they are behind. Bump it in the same commit as
 * any change to a value in docs/tuning/tuning.json, and add its row to the
 * versions table in docs/TUNING.md: `make tuning-export` refuses to write
 * changed values under the old version, and `make check` fails while the
 * export is stale.
 */
#define TUNING_VERSION 4

#endif
