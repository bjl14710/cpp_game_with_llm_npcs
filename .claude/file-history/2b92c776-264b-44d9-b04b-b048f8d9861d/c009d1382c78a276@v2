# Plan: Issue #67 — distance fog and warm directional tint
Date: 2026-07-05 (overnight session 3, autonomous)
Status: READY FOR IMPLEMENTATION
Estimated complexity: M

## Design
One GLSL 330 shader pair (inline strings — no new asset files), two fog
uniforms (fogColor, fogDensity) plus cameraPos per frame. Squared
exponential fog: factor = 1 - exp(-(dist*density)^2), density 0.006 →
~5% haze at 40 units (plaza stays crisp), ~50% at 150, ~80% at the far
corner. fogColor = the sky clear color (135,190,235) so distant geometry
melts into the horizon. Warm tint: texel.rgb * (1.06, 1.00, 0.92) before
fogging — the "warm directional" feel without any lighting system.

## Two draw paths, one shader (key decision)
- Models (buildings, cars, roads, characters): material.shader assigned at
  load in Assets — DrawMesh uses the material's shader.
- Immediate primitives (grass plane, slabs, fountain, viewmodel, emote
  billboards): BeginShaderMode in beginFrame / EndShaderMode in endFrame —
  rlgl's batch uses the current shader. Without this the grass plane would
  stay saturated to the horizon while buildings haze (visible mismatch).
Vertex shader outputs world-space fragPosition via matModel (identity for
batch primitives whose vertices are already world-space — correct either
way).

## Character skinning
raylib 5.5 CPU skinning writes animated vertices into the mesh before
DrawMesh; the custom shader consumes standard attributes only
(position/texcoord/color), so skinning is unaffected. Verified by a
characters screenshot per the issue constraint.

## Changes
- src/app/Assets.{hpp,cpp}: fogShader_ + loc cache, loaded in ctor
  (LoadShaderFromMemory), static uniforms set once, assigned to every
  city-model and character material at load, UnloadShader in dtor;
  accessors for the renderer.
- src/app/RaylibRenderer.cpp: beginFrame sets cameraPos + BeginShaderMode;
  endFrame EndShaderMode before EndMode3D.

## Acceptance
- [ ] Long-street screenshot: far buildings haze toward sky color; plaza
      shot: near objects unchanged.
- [ ] Characters screenshot: skinning intact under the custom shader.
- [ ] Suite green (app-layer only); smoke runs clean.
