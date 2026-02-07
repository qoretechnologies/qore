#!/usr/bin/env node
// render.mjs - Pre-render spinning Q coin animation frames using Three.js
// Generates a C++ header with pre-baked ASCII art frames for qore --version-animation

import createGL from 'gl';
import * as THREE from 'three';
import { readFileSync, writeFileSync } from 'fs';
import { dirname, join } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const PROJECT_ROOT = join(__dirname, '..', '..');

// --- Configuration ---
const ART_WIDTH = 45;
const ART_LINES = 19;
const COIN_DEPTH = 3.0;
const CHAR_ASPECT = 2.0;       // Terminal chars are ~2x taller than wide
const ANIM_FRAMES = 180;
const CELL_PX = 10;            // Supersampling: pixels per character width
const RENDER_W = ART_WIDTH * CELL_PX;                  // 450
const RENDER_H = ART_LINES * Math.round(CELL_PX * CHAR_ASPECT); // 380
const CELL_PX_H = Math.round(CELL_PX * CHAR_ASPECT);  // 20
const ASCII_CHARS = ' .:-=+*#%@';
const BRIGHTNESS_THRESHOLD = 0.04; // Below this, treat as empty space

// --- Q art bitmap (defines the 3D coin shape) ---
const Q_ART = [
    "                  .,l~-?-~l,.                ",
    "             ',l~-]]]]]]]]]-<l,.             ",
    "         ',!~?]]]]?????????]]]]?~!,'         ",
    "     ':!+?]]]]???????]]]???????]]]]?~!,'     ",
    "    !?]]]]??????]]]]]_~-]]]]???????]]]]?!    ",
    "   ']]????]]]]]]]_>;^  .^I>_]]]]]]]????]]'   ",
    "   '???]]]]]]?>;`           ^;<??]]]]]???'   ",
    "   '?]?]]]]?]?.               .?]?]]]]?]?'   ",
    "   '?]?]]]]?]?`               `?]?]]]]?]?'   ",
    "   '?]?]]]]?]?'               '?]?]]]]?]?'   ",
    "   '?]?]]]]?]?^               ^?]?]]]]?]?'   ",
    "   '???]]]]]]]-~!,'      .`,l~-?]]]]]]???'   ",
    "   .?]]????]]]]]]]?~!,'  ;~]]]]]??????]]?.   ",
    '    ">_]]]]]??????]]]]?+i:^`,l~?]]]]]]_>^    ',
    '      .^;>_]]]]]??????]]]]?+!:^"I<>I^.      ',
    "           `;i_?]]]]??????]]]]]_i:`          ",
    "               `:i+?]]]]???????]]]]?~:       ",
    "                   ',!~-]]]]]]]]?+i;^.       ",
    "                       ',!+-?_>;`            ",
];

function parseArt() {
    if (Q_ART.length !== ART_LINES) {
        throw new Error(`Expected ${ART_LINES} art lines, got ${Q_ART.length}`);
    }
    return Q_ART;
}

// --- Character density (0-10) matching C++ char_density ---
function charDensity(c) {
    switch (c) {
        case '.': case ',': case "'": case '`': case '^': return 1;
        case ':': case ';': return 2;
        case '-': case '~': case '!': return 3;
        case '=': case '+': return 4;
        case '*': case 'i': case 'l': return 5;
        case '<': case '>': case 'I': return 6;
        case '?': case '"': return 7;
        case ']': case '[': case '_': return 8;
        case '#': return 9;
        case '%': case '@': return 10;
        default: return 4;
    }
}

// --- Build coin geometry from Q art bitmap ---
function buildCoinGeometry(art) {
    const filled = Array.from({ length: ART_LINES }, (_, y) =>
        Array.from({ length: ART_WIDTH }, (_, x) =>
            x < art[y].length && art[y][x] !== ' '
        )
    );

    const isFilled = (x, y) =>
        x >= 0 && x < ART_WIDTH && y >= 0 && y < ART_LINES && filled[y][x];

    // Count quads needed
    let quadCount = 0;
    for (let y = 0; y < ART_LINES; y++) {
        for (let x = 0; x < ART_WIDTH; x++) {
            if (!filled[y][x]) continue;
            quadCount += 2; // front + back
            if (!isFilled(x - 1, y)) quadCount++;
            if (!isFilled(x + 1, y)) quadCount++;
            if (!isFilled(x, y - 1)) quadCount++;
            if (!isFilled(x, y + 1)) quadCount++;
        }
    }

    // 2 triangles per quad, 3 vertices per triangle
    const vertCount = quadCount * 6;
    const positions = new Float32Array(vertCount * 3);
    const normals = new Float32Array(vertCount * 3);
    const colors = new Float32Array(vertCount * 3);

    let vi = 0;

    function addVertex(pos, normal, color) {
        const base = vi * 3;
        positions[base] = pos[0];
        positions[base + 1] = pos[1];
        positions[base + 2] = pos[2];
        normals[base] = normal[0];
        normals[base + 1] = normal[1];
        normals[base + 2] = normal[2];
        colors[base] = color[0];
        colors[base + 1] = color[1];
        colors[base + 2] = color[2];
        vi++;
    }

    function addQuad(v0, v1, v2, v3, n, c) {
        addVertex(v0, n, c);
        addVertex(v1, n, c);
        addVertex(v2, n, c);
        addVertex(v0, n, c);
        addVertex(v2, n, c);
        addVertex(v3, n, c);
    }

    const halfD = COIN_DEPTH / 2;
    const halfH = ART_LINES * CHAR_ASPECT / 2;
    const edgeColor = [0.65, 0.65, 0.65];

    // Dome curvature: tilt face normals based on distance from center
    // This creates a visible lighting gradient across the face
    const domeStrength = 0.25;
    const maxRadius = Math.max(ART_WIDTH / 2, halfH);

    function domeNormal(cx, cy, zSign) {
        const nx = domeStrength * cx / maxRadius;
        const ny = domeStrength * cy / maxRadius;
        const nz = zSign;
        const len = Math.sqrt(nx * nx + ny * ny + nz * nz);
        return [nx / len, ny / len, nz / len];
    }

    for (let y = 0; y < ART_LINES; y++) {
        for (let x = 0; x < ART_WIDTH; x++) {
            if (!filled[y][x]) continue;

            const density = charDensity(art[y][x]) / 10.0;
            const faceColor = [density, density, density];

            const x1 = x - ART_WIDTH / 2;
            const x2 = x1 + 1;
            const y1 = (ART_LINES - 1 - y) * CHAR_ASPECT - halfH;
            const y2 = y1 + CHAR_ASPECT;
            const z1 = -halfD;
            const z2 = halfD;

            // Cell center for dome normal calculation
            const cx = (x1 + x2) / 2;
            const cy = (y1 + y2) / 2;

            // Front face (+Z) with dome-curved normal
            const frontN = domeNormal(cx, cy, 1);
            addQuad(
                [x1, y1, z2], [x2, y1, z2], [x2, y2, z2], [x1, y2, z2],
                frontN, faceColor
            );
            // Back face (-Z) with dome-curved normal
            const backN = domeNormal(cx, cy, -1);
            addQuad(
                [x2, y1, z1], [x1, y1, z1], [x1, y2, z1], [x2, y2, z1],
                backN, faceColor
            );
            // Left edge (-X)
            if (!isFilled(x - 1, y)) {
                addQuad(
                    [x1, y1, z1], [x1, y1, z2], [x1, y2, z2], [x1, y2, z1],
                    [-1, 0, 0], edgeColor
                );
            }
            // Right edge (+X)
            if (!isFilled(x + 1, y)) {
                addQuad(
                    [x2, y1, z2], [x2, y1, z1], [x2, y2, z1], [x2, y2, z2],
                    [1, 0, 0], edgeColor
                );
            }
            // Top edge (+Y)
            if (!isFilled(x, y - 1)) {
                addQuad(
                    [x1, y2, z2], [x2, y2, z2], [x2, y2, z1], [x1, y2, z1],
                    [0, 1, 0], edgeColor
                );
            }
            // Bottom edge (-Y)
            if (!isFilled(x, y + 1)) {
                addQuad(
                    [x1, y1, z1], [x2, y1, z1], [x2, y1, z2], [x1, y1, z2],
                    [0, -1, 0], edgeColor
                );
            }
        }
    }

    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(positions.slice(0, vi * 3), 3));
    geometry.setAttribute('normal', new THREE.BufferAttribute(normals.slice(0, vi * 3), 3));
    geometry.setAttribute('color', new THREE.BufferAttribute(colors.slice(0, vi * 3), 3));
    return geometry;
}

// --- Read pixel buffer and convert to 45x19 brightness grid ---
function readBrightnessGrid(glCtx) {
    const pixels = new Uint8Array(RENDER_W * RENDER_H * 4);
    glCtx.readPixels(0, 0, RENDER_W, RENDER_H, glCtx.RGBA, glCtx.UNSIGNED_BYTE, pixels);

    const grid = [];
    for (let cy = 0; cy < ART_LINES; cy++) {
        const row = [];
        for (let cx = 0; cx < ART_WIDTH; cx++) {
            let sum = 0;
            let count = 0;
            const px0 = cx * CELL_PX;
            // WebGL pixel buffer is bottom-up; our grid is top-down
            const py0 = (ART_LINES - 1 - cy) * CELL_PX_H;

            for (let dy = 0; dy < CELL_PX_H; dy++) {
                for (let dx = 0; dx < CELL_PX; dx++) {
                    const idx = ((py0 + dy) * RENDER_W + (px0 + dx)) * 4;
                    const r = pixels[idx] / 255;
                    const g = pixels[idx + 1] / 255;
                    const b = pixels[idx + 2] / 255;
                    sum += 0.299 * r + 0.587 * g + 0.114 * b;
                    count++;
                }
            }
            row.push(count > 0 ? sum / count : 0);
        }
        grid.push(row);
    }
    return grid;
}

// --- Map brightness to ASCII character ---
function brightnessToChar(b) {
    if (b < BRIGHTNESS_THRESHOLD) return ' ';
    const idx = Math.round(b * (ASCII_CHARS.length - 1));
    return ASCII_CHARS[Math.max(0, Math.min(ASCII_CHARS.length - 1, idx))];
}

// --- Main ---
function main() {
    console.log('Parsing Q art from command-line.cpp...');
    const art = parseArt();
    console.log(`  Parsed ${art.length} lines, widths: ${art.map(l => l.length).join(',')}`);

    console.log('Building 3D coin geometry...');
    const geometry = buildCoinGeometry(art);
    const triCount = geometry.getAttribute('position').count / 3;
    console.log(`  ${triCount} triangles`);

    console.log(`Creating headless WebGL context (${RENDER_W}x${RENDER_H})...`);
    const glCtx = createGL(RENDER_W, RENDER_H, { preserveDrawingBuffer: true });
    if (!glCtx) {
        throw new Error('Failed to create WebGL context');
    }

    const mockCanvas = {
        width: RENDER_W,
        height: RENDER_H,
        style: {},
        addEventListener: () => {},
        removeEventListener: () => {},
        getContext: () => glCtx,
    };

    const renderer = new THREE.WebGLRenderer({
        canvas: mockCanvas,
        context: glCtx,
        antialias: false,
    });
    renderer.setSize(RENDER_W, RENDER_H);

    // Scene
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x000000);

    const material = new THREE.MeshPhongMaterial({
        vertexColors: true,
        side: THREE.DoubleSide,
        shininess: 80,
        specular: new THREE.Color(0.2, 0.2, 0.2),
    });

    const mesh = new THREE.Mesh(geometry, material);
    scene.add(mesh);

    // Front light: dead-on for maximum brightness when face-on
    const frontLight = new THREE.DirectionalLight(0xffffff, 1.8);
    frontLight.position.set(0, 0, 50);
    scene.add(frontLight);

    // Side accent light: from the right, creates visible directionality during rotation
    const accentLight = new THREE.DirectionalLight(0xffffff, 0.7);
    accentLight.position.set(25, 5, 10);
    scene.add(accentLight);

    // Fill light: back-left — subtle, softens shadows on back face
    const fillLight = new THREE.DirectionalLight(0xffffff, 0.15);
    fillLight.position.set(-10, 5, -15);
    scene.add(fillLight);

    // Hemisphere light: sky/ground gradient for natural top-to-bottom variation
    const hemiLight = new THREE.HemisphereLight(0xffffff, 0x444444, 0.15);
    scene.add(hemiLight);

    // Ambient: enough to keep edges visible
    const ambient = new THREE.AmbientLight(0xffffff, 0.15);
    scene.add(ambient);

    // Camera: orthographic, fits the art area exactly
    const halfW = ART_WIDTH / 2;
    const halfH = ART_LINES * CHAR_ASPECT / 2;
    const camera = new THREE.OrthographicCamera(-halfW, halfW, halfH, -halfH, 0.1, 200);
    camera.position.set(0, 0, 100);
    camera.lookAt(0, 0, 0);

    // Render all frames
    console.log(`Rendering ${ANIM_FRAMES} frames...`);
    const frames = [];

    for (let frame = 0; frame < ANIM_FRAMES; frame++) {
        const t = frame / ANIM_FRAMES;
        const theta = 2 * Math.PI * t + 0.35 * Math.sin(4 * Math.PI * t);
        mesh.rotation.y = theta;

        renderer.render(scene, camera);

        const grid = readBrightnessGrid(glCtx);

        // Convert brightness grid to ASCII
        const frameLines = [];
        for (let y = 0; y < ART_LINES; y++) {
            let line = '';
            for (let x = 0; x < ART_WIDTH; x++) {
                line += brightnessToChar(grid[y][x]);
            }
            frameLines.push(line);
        }
        frames.push(frameLines);

        if ((frame + 1) % 30 === 0) {
            console.log(`  Frame ${frame + 1}/${ANIM_FRAMES}`);
        }
    }

    // Write C++ header
    console.log('Writing C++ header...');
    let header = '';
    header += '// Auto-generated by tools/render-version-art/render.mjs\n';
    header += '// Do not edit manually - re-run the render script to regenerate\n';
    header += '// Copyright (c) 2003 - 2026 David Nichols\n';
    header += '\n';
    header += '#pragma once\n';
    header += '\n';
    header += `static const int ANIM_FRAME_COUNT = ${ANIM_FRAMES};\n`;
    header += `static const int ANIM_FRAME_WIDTH = ${ART_WIDTH};\n`;
    header += `static const int ANIM_FRAME_HEIGHT = ${ART_LINES};\n`;
    header += '\n';
    header += `static const char anim_frames[${ANIM_FRAMES}][${ART_LINES}][${ART_WIDTH + 1}] = {\n`;

    for (let f = 0; f < frames.length; f++) {
        header += `    { // Frame ${f}\n`;
        for (let l = 0; l < frames[f].length; l++) {
            // Pad or trim to exactly ART_WIDTH
            let line = frames[f][l];
            while (line.length < ART_WIDTH) line += ' ';
            line = line.substring(0, ART_WIDTH);
            // Escape for C string
            const escaped = line
                .replace(/\\/g, '\\\\')
                .replace(/"/g, '\\"')
                .replace(/\?/g, '\\?');
            header += `        "${escaped}"`;
            header += l < frames[f].length - 1 ? ',\n' : '\n';
        }
        header += '    }';
        header += f < frames.length - 1 ? ',\n' : '\n';
    }
    header += '};\n';

    const outputPath = join(PROJECT_ROOT, 'version_animation_frames.h');
    writeFileSync(outputPath, header);
    console.log(`Wrote ${outputPath} (${(header.length / 1024).toFixed(1)} KB)`);

    // Preview
    console.log('\nPreview - Frame 0 (face-on):');
    for (const line of frames[0]) {
        console.log('  |' + line + '|');
    }
    console.log('\nPreview - Frame 45 (quarter rotation):');
    for (const line of frames[45]) {
        console.log('  |' + line + '|');
    }
    console.log('\nPreview - Frame 90 (edge-on):');
    for (const line of frames[90]) {
        console.log('  |' + line + '|');
    }

    try {
        renderer.dispose();
    } catch (e) {
        // Headless environment may not have cancelAnimationFrame
    }
    glCtx.getExtension('STACKGL_destroy_context')?.destroy();
    console.log('\nDone!');
}

main();
