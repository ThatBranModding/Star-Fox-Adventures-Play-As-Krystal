#pragma once
#include <cstdint>
#include <cstddef>

typedef struct NativeModelHeader {
    uint8_t refCount;
    uint8_t unk01;
    uint16_t flags;
    uint16_t modelId;
    uint8_t unk06[6];
    int32_t dataSize;
    uint8_t unk10[8];
    uint8_t* unk18;
    uint8_t* unk1C;
    uintptr_t* textureIds;
    uint8_t flags24;
    uint8_t unk25[3];
    uint8_t* vertices;
    uint8_t* normals;
    uint8_t* colors;
    uint8_t* texCoords;
    void* renderOps;
    uint8_t* jointData;
    uint8_t* jointBlendData;
    float vertexAnimPivot[3];
    float vertexAnimScaleDivisor;
    uint8_t* extraJointDefs;
    uint8_t* hitVolumes;
    uint8_t* collisionTriangles;
    uint8_t* collisionBlocks;

    union {
        uint8_t* animationModelPtrs;
        uint8_t** moveData;
    };

    uint8_t* animationDataSection;

    union {
        uint8_t* animationHeaderBuffer;
        int16_t* cachedAnimIds;
    };

    int16_t animGroupBaseIndices[8];
    int32_t animationDataFileOffset;
    int16_t headerSize;
    uint8_t unk86[4];
    uint16_t vertexAnimCount;
    uint8_t unk8C[8];
    uint8_t* vertexAnimEntriesRaw;
    uint8_t unk98[0xC];
    uint8_t* vertexAnimEntries;
    uint8_t* vertexAnimBase;
    uint8_t unkAC[2];
    uint16_t blendAnimCount;
    uint8_t unkB0[8];
    uint8_t* blendAnimEntriesRaw;
    uint8_t unkBC[0xC];
    uint8_t* blendAnimEntries;
    uint8_t* blendAnimBase;
    uint8_t* displayLists;
    uint8_t* instrs;
    uint16_t instrsBitLenWords;
    uint8_t unkDA[2];
    uint8_t** morphTargetPtrs;
    uint16_t cullDistance;
    uint16_t shaderFlags;
    uint16_t vertexCount;
    uint16_t normalCount;
    uint16_t colorCount;
    uint16_t texCoordCount;
    uint16_t animationCount;
    uint8_t unkEE[2];
    uint16_t collisionBlockCount;
    uint8_t textureCount;
    uint8_t jointCount;
    uint8_t extraJointCount;
    uint8_t displayListCount;
    uint8_t shadowDisplayListCount;
    uint8_t hitVolumeCount;
    uint8_t renderOpCount;
    uint8_t morphTargetCount;
    uint8_t texMtxCount;
} NativeModelHeader;

static_assert(sizeof(void*) == 8);
static_assert(offsetof(NativeModelHeader, cachedAnimIds) == 0xB0);
static_assert(offsetof(NativeModelHeader, animGroupBaseIndices) == 0xB8);
