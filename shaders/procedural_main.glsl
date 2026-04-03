void main() {
    vec2 st = (vUV - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    
    // Apply global camera zoom and offset
    st *= uCameraZoom;
    st += vec2(uCameraOffsetX, uCameraOffsetY);

    vec4 color;
    if (uMode == 0) {
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    } else if (uMode == 1) {
        color = renderASCIIOcean(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 2) {
        color = renderSacredGeometry(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 3) {
        color = renderGlitchGrid(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 4) {
        color = renderChemicalFlow(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 5) {
        color = renderCrystalLattice(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 6) {
        color = renderPhantomFractals(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 7) {
        color = renderFractalObject(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 8) {
        color = renderPulsarTunnel(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 9) {
        color = renderAuroraBloom(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 10) {
        color = renderRibbonScanlines(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 11) {
        color = renderNebula(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 12) {
        color = renderKaleidoscopeFractal(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 13) {
        color = renderVoronoiCells(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 14) {
        color = renderRaymarchedObject(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 15) {
        color = renderReactionDiffusionPattern(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 16) {
        color = renderLiquidRefraction(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 17) {
        color = renderStarfieldWarp(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 18) {
        color = renderPlasmaClassic(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 19) {
        color = renderDomainWarpedFractal(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 20) {
        color = renderFractalTunnel(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 21) {
        color = renderVolumetricStarfield(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 22) {
        color = renderVoxelPathTracer(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 23) {
        color = renderEtiennePulse(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 24) {
        color = renderFractalRunway(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 25) {
        color = renderVolumetricTunnel(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 26) {
        color = renderChromaticSwirl(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 27) {
        color = renderHyperPulse(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 28) {
        color = renderGyroidReflections(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 29) {
        color = renderHead(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 30) {
        color = renderMetalGyroidHall(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 31) {
        color = renderHexKaleidoscope(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 32) {
        color = renderHSVColorShift(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 33) {
        color = renderCryptRoots(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 34) {
        color = renderBreathing(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 35) {
        color = renderEvolutionNoise(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 36) {
        color = renderPhiFields(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 37) {
        color = renderFractalInfinity(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 38) {
        color = renderWalker(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 39) {
        color = renderWeirdCreature(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 40) {
        color = renderAnaglyphAssembly(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 41) {
        color = renderMessageTunnel(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 42) {
        color = renderPouetGrid(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 43) {
        color = renderCylinderRepeat(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 44) {
        color = renderPowerParticle(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else if (uMode == 45) {
        color = renderFlopine(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    } else {
        color = renderDomainWarpedFractal(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
    }

    FragColor = color;
}
