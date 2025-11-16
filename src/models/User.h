#pragma once
#include <string>
#include <vector>
#include <chrono>

struct User {
    std::string id;
    std::string username;
    std::string team_name;
    bool is_active;
    
    User(const std::string& id, const std::string& username, 
         const std::string& team_name, bool is_active = true)
        : id(id), username(username), team_name(team_name), is_active(is_active) {}
};

struct Team {
    std::string name;
    std::vector<User> members;
    
    Team(const std::string& name) : name(name) {}
};