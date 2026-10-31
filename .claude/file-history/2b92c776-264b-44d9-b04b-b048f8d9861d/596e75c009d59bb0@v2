#include "Assets.hpp"

namespace llm_npc {

Assets::Assets(const std::string& assetsDir) {
    // TODO(implement): if assetsDir/models is missing → leave loaded_=false
    // and log the tools/fetch_assets.sh hint once. Otherwise:
    //  - LoadModel each curated building .glb (spotId map) + generic kinds
    //  - LoadModel + LoadModelAnimations each character .glb; resolve
    //    idle/walk/gesture clip indexes by animation name
    //  - LoadFontEx(assetsDir/fonts/..., size 32) for crisp UI text
    //  - bake faces: for each NpcFace, FaceTexture::bake(face) → faces_[i]
    (void)assetsDir;
}

Assets::~Assets() {
    // TODO(implement): UnloadModel/UnloadModelAnimations/UnloadTexture/
    // UnloadFont for everything loaded (only when loaded_).
}

const Model* Assets::modelForBuilding(const Building& building) const {
    // TODO(implement): curated spotId lookup first, then
    // facadeKind % genericBuildings_.size(). nullptr when !loaded_.
    (void)building;
    return nullptr;
}

const Model* Assets::modelForCharacter(int variantSeed) const {
    // TODO(implement): characters_[variantSeed % characters_.size()].
    (void)variantSeed;
    return nullptr;
}

const Texture2D* Assets::faceTexture(int npcFace) const {
    // TODO(implement)
    (void)npcFace;
    return nullptr;
}

const Font* Assets::uiFont() const {
    // TODO(implement)
    return nullptr;
}

}  // namespace llm_npc
