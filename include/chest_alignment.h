#pragma once

using ChestPointMatrixFn = float* (*)(void*, int);
static ChestPointMatrixFn g_chestPointMatrix{};
static void* g_chestPoseOwner{};
static float g_chestLocalPose[12]{};
static constexpr float CHEST_STAFF_VERTICAL_OFFSET = -1.55f;

static void chestConcat(const float* a, const float* b, float* out) {
    float result[12]{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col)
            for (int k = 0; k < 3; ++k)
                result[row * 4 + col] += a[row * 4 + k] * b[k * 4 + col];
        result[row * 4 + 3] = a[row * 4 + 3];
        for (int k = 0; k < 3; ++k)
            result[row * 4 + 3] += a[row * 4 + k] * b[k * 4 + 3];
    }
    std::memcpy(out, result, sizeof(result));
}

static bool chestInverse(const float* m, float* out) {
    const float a = m[0], b = m[1], c = m[2], d = m[4], e = m[5], f = m[6], g = m[8], h = m[9],
                i = m[10];
    const float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (!std::isfinite(det) || std::fabs(det) < 1e-8f)
        return false;
    float result[12]{(e * i - f * h) / det,
                     (c * h - b * i) / det,
                     (b * f - c * e) / det,
                     0,
                     (f * g - d * i) / det,
                     (a * i - c * g) / det,
                     (c * d - a * f) / det,
                     0,
                     (d * h - e * g) / det,
                     (b * g - a * h) / det,
                     (a * e - b * d) / det,
                     0};
    for (int row = 0; row < 3; ++row)
        for (int k = 0; k < 3; ++k)
            result[row * 4 + 3] -= result[row * 4 + k] * m[k * 4 + 3];
    for (float value : result)
        if (!std::isfinite(value))
            return false;
    std::memcpy(out, result, sizeof(result));
    return true;
}

static bool chestDescendant(const uint8_t* bones, int count, int bone, int root) {
    for (int depth = 0; depth < count && bone >= 0 && bone < count; ++depth) {
        if (bone == root)
            return true;
        bone = *reinterpret_cast<const int8_t*>(bones + bone * 0x1c);
    }
    return false;
}

static bool chestUpperBodyTransform(NativeModel* model, int rootSlot, const float* transform) {
    if (!model || !model->file || !model->file->jointData || !model->matrices[model->flags & 1])
        return false;
    const auto* bones = model->file->jointData;
    const int count = model->file->jointCount;
    int root = -1;
    for (int bone = 0; bone < count; ++bone)
        if ((bones[bone * 0x1c + 1] & 0x7f) == rootSlot) {
            root = bone;
            break;
        }
    if (root <= 0)
        return false;

    int children = 0;
    for (int bone = 0; bone < count; ++bone)
        if (*reinterpret_cast<const int8_t*>(bones + bone * 0x1c) == root)
            ++children;
    if (children < 3)
        return false;
    auto* matrices = static_cast<float*>(model->matrices[model->flags & 1]);
    bool changed[256]{};
    for (int bone = 0; bone < count; ++bone) {
        if (!chestDescendant(bones, count, bone, root))
            continue;
        for (int channel = 1; channel <= 3; ++channel) {
            const int slot = bones[bone * 0x1c + channel] & 0x7f;
            if (slot >= count + model->file->extraJointCount || changed[slot])
                continue;
            chestConcat(transform, matrices + slot * 16, matrices + slot * 16);
            changed[slot] = true;
        }
    }
    return true;
}

static bool chestPoseEligible(void* obj, NativeModel* model) {
    if (!g_chestPointMatrix || !obj || !model || !model->file || !model->animA ||
        !krystalGameplayActive(obj) || !playerSequenceActive(obj))
        return false;
    const auto* bytes = static_cast<const uint8_t*>(obj);
    if (*reinterpret_cast<const int8_t*>(bytes + OBJ_BANK_INDEX_OFFSET) != 0)
        return false;
    const int move = *reinterpret_cast<const int16_t*>(bytes + 0xe0);
    if (move != 0x250 && !(obj == g_chestHeightPlayer && obj == g_chestPoseOwner))
        return false;
    auto** banks =
        *reinterpret_cast<NativeModel***>(static_cast<uint8_t*>(obj) + OBJ_MODEL_BANKS_OFFSET);
    return banks && banks[0] == model && banks[1] && banks[1]->file &&
           model->file->modelId == KRYSTAL_MODEL_ID && banks[1]->file->modelId == 1 &&
           model->file->jointCount > 0 && banks[1]->file->jointCount > 0 &&
           model->file->jointCount + model->file->extraJointCount <= 128 &&
           banks[1]->file->jointCount + banks[1]->file->extraJointCount <= 128;
}

static void chestPoseMatrices(void* bytes, void* header, void* obj, float* world) {
    auto* model = static_cast<NativeModel*>(bytes);
    if (!world || header != (model ? model->file : nullptr) || !chestPoseEligible(obj, model)) {
        g_updateAnimMatrices(bytes, header, obj, world);
        return;
    }
    float target[12]{};
    const int move = *reinterpret_cast<const int16_t*>(static_cast<const uint8_t*>(obj) + 0xe0);
    if (move == 0x250) {
        auto** banks =
            *reinterpret_cast<NativeModel***>(static_cast<uint8_t*>(obj) + OBJ_MODEL_BANKS_OFFSET);
        NativeModel foxPose = *model;
        foxPose.file = banks[1]->file;
        alignas(16) float storage[2][128 * 16]{};
        foxPose.matrices[0] = storage[0];
        foxPose.matrices[1] = storage[1];
        NativeModel* poseBanks[]{model, &foxPose};
        alignas(8) uint8_t actor[0x110];
        std::memcpy(actor, obj, sizeof(actor));
        *reinterpret_cast<NativeModel***>(actor + OBJ_MODEL_BANKS_OFFSET) = poseBanks;
        actor[OBJ_BANK_INDEX_OFFSET] = 1;
        float vanillaWorld[16];
        std::memcpy(vanillaWorld, world, sizeof(vanillaWorld));
        for (int index : {1, 5, 9})
            vanillaWorld[index] /= KRYSTAL_CHEST_HEIGHT_SCALE;
        for (int index : {0, 2, 4, 6, 8, 10})
            vanillaWorld[index] /= KRYSTAL_CHEST_REACH_SCALE;
        g_updateAnimMatrices(&foxPose, foxPose.file, actor, vanillaWorld);
        float* mount = g_chestPointMatrix(actor, 2);
        if (mount) {
            std::memcpy(target, mount, sizeof(target));
            target[7] += CHEST_STAFF_VERTICAL_OFFSET;
        }
    }
    g_updateAnimMatrices(bytes, header, obj, world);
    float* current = g_chestPointMatrix(obj, 2);
    auto* base = static_cast<float*>(model->matrices[model->flags & 1]);
    if (!current || !base)
        return;
    const ptrdiff_t slot = (current - base) / 16;
    if (slot <= 0 || slot >= model->file->jointCount + model->file->extraJointCount)
        return;
    float worldInverse[12], adjustment[12], temp[12];
    if (!chestInverse(world, worldInverse))
        return;
    if (move == 0x250) {
        float inverse[12];
        if (!chestInverse(current, inverse))
            return;
        chestConcat(target, inverse, adjustment);
    } else {
        chestConcat(world, g_chestLocalPose, temp);
        chestConcat(temp, worldInverse, adjustment);
    }
    if (!chestUpperBodyTransform(model, int(slot), adjustment))
        return;
    if (move == 0x250) {
        chestConcat(worldInverse, adjustment, temp);
        chestConcat(temp, world, g_chestLocalPose);
        g_chestPoseOwner = obj;
    }
}
