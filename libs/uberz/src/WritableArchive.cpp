/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <uberz/WritableArchive.h>

#include <utils/Log.h>

using namespace utils;

struct Keyword {
    const char* const txt;
    const size_t len;
    bool test(const char* cursor) const { return strncmp(cursor, txt, len) == 0; }
};

#define DEFINE_KEYWORD(a, b) static const Keyword a = { b, strlen(b) }

namespace Keywords {
DEFINE_KEYWORD(BlendingMode, "BlendingMode");
DEFINE_KEYWORD(ShadingModel, "ShadingModel");

DEFINE_KEYWORD(unsupported, "unsupported");
DEFINE_KEYWORD(optional, "optional");
DEFINE_KEYWORD(required, "required");

DEFINE_KEYWORD(opaque, "opaque");
DEFINE_KEYWORD(transparent, "transparent");
DEFINE_KEYWORD(add, "add");
DEFINE_KEYWORD(masked, "masked");
DEFINE_KEYWORD(fade, "fade");
DEFINE_KEYWORD(multiply, "multiply");
DEFINE_KEYWORD(screen, "screen");

DEFINE_KEYWORD(unlit, "unlit");
DEFINE_KEYWORD(lit, "lit");
DEFINE_KEYWORD(subsurface, "subsurface");
DEFINE_KEYWORD(cloth, "cloth");
DEFINE_KEYWORD(specular_glossiness, "specular_glossiness");
}

namespace filament::uberz {

static size_t consumeArchiveFeature(const char* cursor, ArchiveFeature* feature) {
    if (Keywords::unsupported.test(cursor)) {
        *feature = ArchiveFeature::UNSUPPORTED;
        return Keywords::unsupported.len;
    }
    if (Keywords::required.test(cursor)) {
        *feature = ArchiveFeature::REQUIRED;
        return Keywords::required.len;
    }
    if (Keywords::optional.test(cursor)) {
        *feature = ArchiveFeature::OPTIONAL;
        return Keywords::optional.len;
    }
    return 0;
}

static size_t consumeBlendingMode(const char* cursor, BlendingMode* blending) {
    if (Keywords::opaque.test(cursor)) {
        *blending = BlendingMode::OPAQUE;
        return Keywords::opaque.len;
    }
    if (Keywords::transparent.test(cursor)) {
        *blending = BlendingMode::TRANSPARENT;
        return Keywords::transparent.len;
    }
    if (Keywords::add.test(cursor)) {
        *blending = BlendingMode::ADD;
        return Keywords::add.len;
    }
    if (Keywords::masked.test(cursor)) {
        *blending = BlendingMode::MASKED;
        return Keywords::masked.len;
    }
    if (Keywords::fade.test(cursor)) {
        *blending = BlendingMode::FADE;
        return Keywords::fade.len;
    }
    if (Keywords::multiply.test(cursor)) {
        *blending = BlendingMode::MULTIPLY;
        return Keywords::multiply.len;
    }
    if (Keywords::screen.test(cursor)) {
        *blending = BlendingMode::SCREEN;
        return Keywords::screen.len;
    }
    return 0;
}

static size_t consumeShadingModel(const char* cursor, Shading* shading) {
    if (Keywords::unlit.test(cursor)) {
        *shading = Shading::UNLIT;
        return Keywords::unlit.len;
    }
    if (Keywords::lit.test(cursor)) {
        *shading = Shading::LIT;
        return Keywords::lit.len;
    }
    if (Keywords::subsurface.test(cursor)) {
        *shading = Shading::SUBSURFACE;
        return Keywords::subsurface.len;
    }
    if (Keywords::cloth.test(cursor)) {
        *shading = Shading::CLOTH;
        return Keywords::cloth.len;
    }
    if (Keywords::specular_glossiness.test(cursor)) {
        *shading = Shading::SPECULAR_GLOSSINESS;
        return Keywords::specular_glossiness.len;
    }
    return 0;
}

void WritableArchive::addMaterial(const char* name, const uint8_t* package, size_t packageSize) {
    mMaterials[++mMaterialIndex] = {
        CString(name),
        FixedCapacityVector<uint8_t>(packageSize)
    };
    memcpy(mMaterials[mMaterialIndex].package.data(), package, packageSize);
    mLineNumber = 1;
}

void WritableArchive::addSpecLine(const char* line) {
    const char* cursor = line;

    assert_invariant(mMaterialIndex > -1);

    auto emitSyntaxError = [&](const char* msg) {
        const auto& material = mMaterials[mMaterialIndex];
        const int column = 1 + cursor - line;
        slog.e
            << material.name.c_str() << ".spec(" << mLineNumber << ","  << column << "): "
            << msg << io::endl;
        exit(1);
    };

    auto isAlphaNumeric = [](char c) -> bool {
        return (c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '_';
    };

    auto isSymbol = [&](char c) -> bool {
        return cursor[0] == c;
    };

    auto isWhitespace = [&](char c) -> bool {
        return c == ' ' || c == '\t';
    };

    auto consumeIdentifier = [&]() -> size_t {
        size_t i = 0;
        while (isAlphaNumeric(cursor[i])) {
            i++;
        }
        return i;
    };

    auto consumeWhitespace = [&]() -> size_t {
        size_t i = 0;
        while (isWhitespace(cursor[i])) {
            i++;
        }
        return i;
    };

    auto parseFeatureFlag = [&]() {
        size_t length = consumeIdentifier();
        if (length == 0) {
            emitSyntaxError("expected identifier");
        }
        CString id(cursor, length);
        cursor += length;
        cursor += consumeWhitespace();
        if (!isSymbol('=')) {
            emitSyntaxError("expected equal sign");
        }
        ++cursor;
        cursor += consumeWhitespace();
        ArchiveFeature feature;
        length = consumeArchiveFeature(cursor, &feature);
        if (length == 0) {
            emitSyntaxError("expected unsupported / optional / required");
        }
        cursor += length;
        if (cursor[0] != 0) {
            emitSyntaxError("unexpected trailing character(s)");
        }
        #warning TODO: set prop in current material
    };

    auto parseBlendingMode = [&]() {
        cursor += consumeWhitespace();
        if (!isSymbol('=')) {
            emitSyntaxError("expected equal sign");
        }
        ++cursor;
        BlendingMode blending;
        size_t length = consumeBlendingMode(cursor, &blending);
        if (length == 0) {
            emitSyntaxError("expected lowercase blending mode enum");
        }
        if (cursor[0] != 0) {
            emitSyntaxError("unexpected trailing character(s)");
        }
        #warning TODO: set prop in current material
    };

    auto parseShadingModel = [&]() {
        cursor += consumeWhitespace();
        if (!isSymbol('=')) {
            emitSyntaxError("expected equal sign");
        }
        ++cursor;
        Shading shading;
        size_t length = consumeShadingModel(cursor, &shading);
        if (length == 0) {
            emitSyntaxError("expected lowercase shading enum");
        }
        if (cursor[0] != 0) {
            emitSyntaxError("unexpected trailing character(s)");
        }
        #warning TODO: set prop in current material
    };

    if (!line || !line[0] || line[0] == '\n' || line[0] == '#') {
        // Do nothing for comment or empty line
    } else if (Keywords::BlendingMode.test(line)) {
        cursor += Keywords::BlendingMode.len;
        parseBlendingMode();
    } else if (Keywords::ShadingModel.test(line)) {
        cursor += Keywords::ShadingModel.len;
        parseShadingModel();
    } else {
        parseFeatureFlag();
    }
    ++mLineNumber;
}

FixedCapacityVector<uint8_t> WritableArchive::serialize() const {
    FixedCapacityVector<uint8_t> foo(5);
    // TODO
    return foo;
}

} // namespace filament::uberz
