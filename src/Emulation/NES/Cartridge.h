#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

#include "Mappers/Mapper.h"

void LoadCardDb(const std::string& path);
std::shared_ptr<Mapper> LoadCart(const std::string& path);
