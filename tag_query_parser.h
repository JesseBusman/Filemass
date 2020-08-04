#pragma once

#include "tag_query.h"

std::shared_ptr<TagQuery> parseTagQuery(const std::string& str);
