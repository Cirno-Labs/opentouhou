#pragma once

#include "th01/gameplay/assets.h"

// Shared backing for the synthetic assets of each boss.
typedef struct {
    const GAMEPLAY_ASSETS *assets;
    uint8_t *pixels;
    uint8_t *coverage;
    PI_IMAGE image;
} TH01_TEST_BOSS_BACKING;

// Creates complete, synthetic assets for public construction and lifecycle
// tests. All bytes and mutable fixture backing belong to allocator.
GAMEPLAY_ASSETS *Th01TestAssets_Create(const ALLOCATOR *allocator);

// Create one boss's synthetic assets. Each lives in its own file, with the
// boss's source.
const void *Th01TestAssets_Singyoku(
    const ALLOCATOR *a, const TH01_TEST_BOSS_BACKING *backing);
const void *Th01TestAssets_YuugenMagan(
    const ALLOCATOR *a, const TH01_TEST_BOSS_BACKING *backing);
const void *Th01TestAssets_Mima(
    const ALLOCATOR *a, const TH01_TEST_BOSS_BACKING *backing);
const void *Th01TestAssets_Kikuri(
    const ALLOCATOR *a, const TH01_TEST_BOSS_BACKING *backing);
const void *Th01TestAssets_Elis(
    const ALLOCATOR *a, const TH01_TEST_BOSS_BACKING *backing);
const void *Th01TestAssets_Sariel(
    const ALLOCATOR *a, const TH01_TEST_BOSS_BACKING *backing);
const void *Th01TestAssets_Konngara(
    const ALLOCATOR *a, const TH01_TEST_BOSS_BACKING *backing);
