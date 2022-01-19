#include "World.h"

#include "../Util/Math.h"

World::World(int32_t seed) : generator(seed) {
  shader = AssetManager::instance().loadShaderProgram("assets/shaders/default");
  setTextureAtlas(AssetManager::instance().loadTexture("assets/textures/default_texture.png"));
}

Ref<Chunk> World::generateOrLoadChunk(glm::ivec2 position) {
  Ref<Chunk> chunk = std::make_shared<Chunk>(position);
  generator.populateChunk(chunk);

  std::array<glm::ivec2, 4> chunksAround = {{{0, 16}, {16, 0}, {0, -16}, {-16, 0}}};
  for (const glm::ivec2& offset: chunksAround) {
    glm::ivec2 neighborPosition = position + offset;

    if (!isChunkLoaded(neighborPosition))
      continue;

    chunks[neighborPosition]->setDirty();
  }

  return chunk;
}

void World::update(const glm::vec3& playerPosition, float deltaTime) {
  textureAnimation += deltaTime * TextureAnimationSpeed;

  glm::vec2 playerChunkPosition = getChunkIndex(playerPosition);

  auto chunksCopy = chunks;
  float unloadDistance = static_cast<float>(viewDistance + 1) * 16 + 8.0f;
  for (const auto& [chunkPosition, chunk]: chunksCopy) {
    if (glm::abs(glm::distance(glm::vec2(chunkPosition), playerChunkPosition)) > unloadDistance) {
      chunks.erase(chunkPosition);
    }
  }

  float loadDistance = static_cast<float>(viewDistance) * 16 + 8.0f;
  for (int32_t i = -viewDistance; i < viewDistance; i++) {
    for (int32_t j = -viewDistance; j < viewDistance; j++) {
      glm::ivec2 position = glm::ivec2(i * 16, j * 16) + glm::ivec2(playerChunkPosition);
      if (isChunkLoaded(position)) {
        continue;
      }

      float distance = glm::abs(glm::distance(glm::vec2(position), playerChunkPosition));
      if (distance <= loadDistance) {
        chunks[position] = generateOrLoadChunk(position);
      }
    }
  }
}

void World::render(glm::vec3 playerPos, glm::mat4 transform) {
  // todo sort the chunks before rendering

  glm::vec2 animation{0};
  int32_t animationProgress = static_cast<int32_t>(textureAnimation) % 5;

  switch (animationProgress) {
    case 1:
      animation = glm::vec2(1, 0);
      break;
    case 2:
      animation = glm::vec2(2, 0);
      break;
    case 3:
      animation = glm::vec2(1, 1);
      break;
    case 4:
      animation = glm::vec2(2, 1);
      break;
  }

  shader->setVec2("textureAnimation", animation);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  for (auto& [position, chunk]: chunks) { chunk->render(transform, *this); }

  glDisable(GL_BLEND);
}

BlockData World::getBlockAt(glm::ivec3 position) {
  return getChunk(getChunkIndex(position))->getBlockAt(Chunk::toChunkCoordinates(position));
}

bool World::isValidBlockPosition(glm::ivec3 position) {
  return Chunk::isValidPosition(position);
}

bool World::placeBlock(BlockData block, glm::ivec3 position) {
  if (!Chunk::isValidPosition(position)) {
    return false;
  }

  glm::ivec3 positionInChunk = Chunk::toChunkCoordinates(position);
  getChunk(getChunkIndex(position))->placeBlock(block, positionInChunk);

  const std::array<glm::ivec3, 4> blocksAround = {{{0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {-1, 0, 0}}};
  for (const glm::ivec3& offset: blocksAround) {
    glm::ivec3 neighbor = offset + positionInChunk;
    if (!Chunk::isInBounds(neighbor.x, neighbor.y, neighbor.z)) {
      getChunk(getChunkIndex(position + offset))->setDirty();
    }
  }

  return true;
}
glm::ivec2 World::getChunkIndex(glm::ivec3 position) {
  return {position.x - Math::positiveMod(position.x, Chunk::HorizontalSize),
          position.z - Math::positiveMod(position.z, Chunk::HorizontalSize)};
}


Ref<Chunk> World::getChunk(glm::ivec2 position) {
  if (!isChunkLoaded(position)) {
    addChunk(position, generateOrLoadChunk(position));
  }

  return chunks.at(position);
}

void World::setTextureAtlas(const Ref<const Texture>& texture) {
  textureAtlas = texture;
  shader->setTexture("atlas", textureAtlas, 0);
}

std::optional<BlockData> World::getBlockAtIfLoaded(glm::ivec3 position) const {
  glm::ivec2 index = getChunkIndex(position);
  if (!isChunkLoaded(index)) {
    return {};
  }

  return chunks.at(index)->getBlockAt(Chunk::toChunkCoordinates(position));
}

bool World::isChunkLoaded(glm::ivec2 position) const {
  return chunks.contains(position);
}
