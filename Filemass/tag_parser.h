#pragma once

#include <memory>
#include <string>
#include <vector>

#include "tag.h"

std::vector<std::shared_ptr<Tag>> parseTag(std::string str);
