#pragma once

#include <cubos/engine/prelude.hpp>

struct Player
{
    CUBOS_REFLECT;

    float speed{1.0F};           // Speed of the player
    float laneWidth{1.0F};       // Width of the lane
    int lane{0};                 // Current lane
    int targetLane{0};           // Target lane
    bool armoured;               // Wheater player is protected against next hit 
    bool hasJetpack;             // Wheater player is flying in a jetpack
    float jetpackTimer;          // Timer for the jetpack power-up
    float jetpackDuration{1.0F}; // Duration of one jetpack power-up
    float targetY;               // Target vertical position
    float jetpackHeight;         // Max jetpack height
    float verticalSpeed{1.0F};   // Vertical speed of the player
};

void playerPlugin(cubos::engine::Cubos& cubos);
