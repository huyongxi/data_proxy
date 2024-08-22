#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <thread>
#include <string_view>
#include <fmt/core.h>

using std::string;
using std::unordered_map;
using std::shared_ptr;
using std::mutex;
using std::string_view;
using std::unordered_set;


struct InternalMessage
{
    string name;
    string data;
};