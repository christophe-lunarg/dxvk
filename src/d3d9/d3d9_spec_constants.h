#pragma once

#include <array>

#include <cstdint>

#include "../spirv/spirv_module.h"

class D3D9DeviceEx;

namespace dxvk {

  enum D3D9SpecConstantId : uint32_t {
    SpecSamplerType,        // 2 bits for 16 PS samplers      | Bits: 32

    SpecSamplerDepthMode,   // 1 bit for 20 VS + PS samplers  | Bits: 20
    SpecAlphaCompareOp,     // Range: 0 -> 7                  | Bits: 3
    SpecPointMode,          // Range: 0 -> 3                  | Bits: 2
    SpecVertexFogMode,      // Range: 0 -> 3                  | Bits: 2
    SpecPixelFogMode,       // Range: 0 -> 3                  | Bits: 2
    SpecFogEnabled,         // Range: 0 -> 1                  | Bits: 1

    SpecSamplerNull,        // 1 bit for 20 samplers          | Bits: 20
    SpecProjectionType,     // 1 bit for 6 PS 1.x samplers    | Bits: 6

    SpecVertexShaderBools,  // 16 bools                       | Bits: 16
    SpecPixelShaderBools,   // 16 bools                       | Bits: 16

    SpecFetch4,             // 1 bit for 16 PS samplers       | Bits: 16

    SpecConstantCount,
  };

  struct BitfieldPosition {
    constexpr uint32_t mask() const {
      return uint32_t((1ull << sizeInBits) - 1) << bitOffset;
    }

    uint32_t dwordOffset;
    uint32_t bitOffset;
    uint32_t sizeInBits;
  };

  struct D3D9SpecializationInfo {
    static constexpr uint32_t MaxSpecDwords = 5;

    static constexpr std::array<BitfieldPosition, SpecConstantCount> Layout{{
      { 0, 0, 32 },  // SamplerType
      
      { 1, 0,  20 }, // SamplerDepthMode
      { 1, 20, 3 },  // AlphaCompareOp
      { 1, 23, 2 },  // PointMode
      { 1, 25, 2 },  // VertexFogMode
      { 1, 27, 2 },  // PixelFogMode
      { 1, 29, 1 },  // FogEnabled

      { 2, 0,  20 }, // SamplerNull
      { 2, 20, 6 },  // ProjectionType

      { 3, 0,  16 }, // VertexShaderBools
      { 3, 16, 16 }, // PixelShaderBools

      { 4, 0,  16 }, // Fetch4
    }};

    template <D3D9SpecConstantId Id, typename T>
    bool set(const T& value) {
      const uint32_t x = uint32_t(value);
      if (get<Id>() == x)
        return false;

      constexpr auto& layout = Layout[Id];

      data[layout.dwordOffset] &= ~layout.mask();
      data[layout.dwordOffset] |= (x << layout.bitOffset) & layout.mask();

      return true;
    }

    template <D3D9SpecConstantId Id>
    uint32_t get() const {
      constexpr auto& layout = Layout[Id];

      return (data[layout.dwordOffset] & layout.mask()) >> layout.bitOffset;
    }

    std::array<uint32_t, MaxSpecDwords> data = {};
  };

  class D3D9ShaderSpecConstantManager {
  public:
    uint32_t get(SpirvModule &module, uint32_t specUbo, D3D9SpecConstantId id) {
      return get(module, specUbo, id, 0, 32);
    }

    uint32_t get(SpirvModule &module, uint32_t specUbo, D3D9SpecConstantId id, uint32_t bitOffset, uint32_t bitCount) {
      const auto &layout = D3D9SpecializationInfo::Layout[id];

      uint32_t uintType = module.defIntType(32, 0);
      uint32_t optimized = getOptimizedBool(module);

      uint32_t quickValue     = getSpecUBODword(module, specUbo, layout.dwordOffset);
      uint32_t optimizedValue = getSpecConstDword(module, layout.dwordOffset);

      uint32_t val = module.opSelect(uintType, optimized, optimizedValue, quickValue);
      bitCount = std::min(bitCount, layout.sizeInBits - bitOffset);

      if (bitCount == 32)
        return val;

      return module.opBitFieldUExtract(
        module.defIntType(32, 0), val,
        module.consti32(bitOffset + layout.bitOffset),
        module.consti32(bitCount));
    }

  private:
    uint32_t getSpecConstDword(SpirvModule &module, uint32_t idx) {
      if (!m_specConstantIds[idx]) {
        m_specConstantIds[idx] = module.specConst32(module.defIntType(32, 0), 0);
        module.decorateSpecId(m_specConstantIds[idx], idx);
      }

      return m_specConstantIds[idx];
    }

    uint32_t getSpecUBODword(SpirvModule& module, uint32_t specUbo, uint32_t idx) {
      uint32_t uintType = module.defIntType(32, 0);
      uint32_t uintPtr  = module.defPointerType(uintType, spv::StorageClassUniform);

      uint32_t member = module.constu32(idx);
      uint32_t dword  = module.opLoad(uintType, module.opAccessChain(uintPtr, specUbo, 1, &member));

      return dword;
    }

    uint32_t getOptimizedBool(SpirvModule& module) {
      uint32_t boolType = module.defBoolType();

      // The spec constant at MaxNumSpecConstants is set to True
      // when this is an optimized pipeline.
      uint32_t optimized = getSpecConstDword(module, MaxNumSpecConstants);
      optimized = module.opINotEqual(boolType, optimized, module.constu32(0));

      return optimized;
    }

    std::array<uint32_t, MaxNumSpecConstants + 1> m_specConstantIds = {};
  };

}