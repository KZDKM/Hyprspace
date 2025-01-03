#pragma once

#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/pass/PassElement.hpp>

class CRenderModif : public IPassElement {
  public:
    struct SModifData {
        std::optional<SRenderModifData> renderModif;
    };

    CRenderModif(const SModifData& data);
    virtual ~CRenderModif() = default;

    virtual void draw(const CRegion& damage);
    virtual bool needsLiveBlur() { return false; };
    virtual bool needsPrecomputeBlur() { return false; };
    virtual bool undiscardable() { return true; };

    virtual const char* passName() {
        return "CRenderModif";
    }

    private:
    SModifData data;
};

class CRenderRect : public IPassElement {
  public:
    struct SRectData {
        CBox        box;
        CHyprColor  color;
        int         round = 0;
        bool        blur = false, xray = false;
        float       blurA = 1.F;
    };

    CRenderRect(const SRectData& data);
    virtual ~CRenderRect() = default;

    virtual void draw(const CRegion& damage);
    virtual bool needsLiveBlur();
    virtual bool needsPrecomputeBlur();
    virtual std::optional<CBox> boundingBox();
    virtual CRegion opaqueRegion();

    virtual const char* passName() {
        return "CRenderRect";
    }

    private:
    SRectData data;
};

class CRenderBorder : public IPassElement {
  public:
    struct SBorderData {
        CBox               box;
        CGradientValueData grad1, grad2;
        bool               hasGrad = false, hasGrad2 = false;
        float              lerp = 0.F, a = 1.F;
        int                round = 0, borderSize = 1, outerRound = -1;
    };

    CRenderBorder(const SBorderData& data);
    virtual ~CRenderBorder() = default;

    virtual void draw(const CRegion& damage);
    virtual bool needsLiveBlur() { return false; };
    virtual bool needsPrecomputeBlur() { return false; };

    virtual const char* passName() {
        return "CRenderBorder";
    }

    private:
    SBorderData data;
};
