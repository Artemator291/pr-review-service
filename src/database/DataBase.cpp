#include "Database.h"
#include <stdexcept>
#include <iostream>
#include <sstream>

Database& Database::getInstance() {
    static Database instance;
    return instance;
}

bool Database::connect(const std::string& connectionString) {
    connection_ = PQconnectdb(connectionString.c_str());
    if (PQstatus(connection_) != CONNECTION_OK) {
        std::cerr << "Database connection failed: " << PQerrorMessage(connection_) << std::endl;
        return false;
    }
    std::cout << "Connected to PostgreSQL database" << std::endl;
    return true;
}

void Database::disconnect() {
    if (connection_) {
        PQfinish(connection_);
        connection_ = nullptr;
    }
}

bool Database::isConnected() const {
    return connection_ && PQstatus(connection_) == CONNECTION_OK;
}

int Database::getTeamId(const std::string& teamName) {
    const char* params[1] = {teamName.c_str()};
    PGresult* res = PQexecParams(connection_,
        "SELECT id FROM teams WHERE name = $1", 1, nullptr, params, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return -1;
    }
    
    int teamId = std::stoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return teamId;
}

bool Database::createTeam(const Team& team) {
    const char* params[1] = {team.name.c_str()};
    PGresult* res = PQexecParams(connection_,
        "INSERT INTO teams (name) VALUES ($1) ON CONFLICT (name) DO NOTHING",
        1, nullptr, params, nullptr, nullptr, 0);
    
    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    
    if (success) {
        int teamId = getTeamId(team.name);
        if (teamId != -1) {
            for (const auto& member : team.members) {
                createOrUpdateUser(member);
            }
        }
    }
    
    return success;
}

std::unique_ptr<Team> Database::getTeam(const std::string& teamName) {
    const char* params[1] = {teamName.c_str()};
    PGresult* res = PQexecParams(connection_,
        "SELECT t.name, u.id, u.username, u.is_active "
        "FROM teams t LEFT JOIN users u ON t.id = u.team_id "
        "WHERE t.name = $1", 1, nullptr, params, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return nullptr;
    }
    
    auto team = std::make_unique<Team>(PQgetvalue(res, 0, 0));
    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetvalue(res, i, 1) != nullptr) {
            team->members.emplace_back(
                PQgetvalue(res, i, 1),
                PQgetvalue(res, i, 2),
                teamName,
                PQgetvalue(res, i, 3)[0] == 't'
            );
        }
    }
    
    PQclear(res);
    return team;
}

bool Database::teamExists(const std::string& teamName) {
    return getTeamId(teamName) != -1;
}

bool Database::createOrUpdateUser(const User& user) {
    int teamId = getTeamId(user.team_name);
    if (teamId == -1) return false;
    
    const char* params[4] = {
        user.id.c_str(),
        user.username.c_str(),
        std::to_string(teamId).c_str(),
        user.is_active ? "true" : "false"
    };
    
    PGresult* res = PQexecParams(connection_,
        "INSERT INTO users (id, username, team_id, is_active) "
        "VALUES ($1, $2, $3, $4) "
        "ON CONFLICT (id) DO UPDATE SET "
        "username = EXCLUDED.username, team_id = EXCLUDED.team_id, is_active = EXCLUDED.is_active",
        4, nullptr, params, nullptr, nullptr, 0);
    
    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    return success;
}

bool Database::setUserActive(const std::string& userId, bool isActive) {
    const char* params[2] = {
        isActive ? "true" : "false",
        userId.c_str()
    };
    
    PGresult* res = PQexecParams(connection_,
        "UPDATE users SET is_active = $1 WHERE id = $2",
        2, nullptr, params, nullptr, nullptr, 0);
    
    bool success = PQresultStatus(res) == PGRES_COMMAND_OK && PQcmdTuples(res)[0] != '0';
    PQclear(res);
    return success;
}

std::unique_ptr<User> Database::getUser(const std::string& userId) {
    const char* params[1] = {userId.c_str()};
    PGresult* res = PQexecParams(connection_,
        "SELECT u.id, u.username, t.name, u.is_active "
        "FROM users u JOIN teams t ON u.team_id = t.id "
        "WHERE u.id = $1", 1, nullptr, params, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return nullptr;
    }
    
    auto user = std::make_unique<User>(
        PQgetvalue(res, 0, 0),
        PQgetvalue(res, 0, 1),
        PQgetvalue(res, 0, 2),
        PQgetvalue(res, 0, 3)[0] == 't'
    );
    
    PQclear(res);
    return user;
}

std::vector<User> Database::getActiveTeamMembers(const std::string& teamName, const std::string& excludeUserId) {
    int teamId = getTeamId(teamName);
    if (teamId == -1) return {};
    
    const char* params[2] = {
        std::to_string(teamId).c_str(),
        excludeUserId.c_str()
    };
    
    PGresult* res = PQexecParams(connection_,
        "SELECT id, username, is_active FROM users "
        "WHERE team_id = $1 AND is_active = true AND id != $2",
        2, nullptr, params, nullptr, nullptr, 0);
    
    std::vector<User> members;
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            members.emplace_back(
                PQgetvalue(res, i, 0),
                PQgetvalue(res, i, 1),
                teamName,
                PQgetvalue(res, i, 2)[0] == 't'
            );
        }
    }
    
    PQclear(res);
    return members;
}

bool Database::createPullRequest(const PullRequest& pr) {
    const char* params[3] = {
        pr.id.c_str(),
        pr.name.c_str(),
        pr.author_id.c_str()
    };
    
    PGresult* res = PQexecParams(connection_,
        "INSERT INTO pull_requests (id, name, author_id) VALUES ($1, $2, $3)",
        3, nullptr, params, nullptr, nullptr, 0);
    
    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    
    if (success && !pr.assigned_reviewers.empty()) {
        for (const auto& reviewer : pr.assigned_reviewers) {
            const char* reviewerParams[2] = {pr.id.c_str(), reviewer.c_str()};
            PGresult* revRes = PQexecParams(connection_,
                "INSERT INTO pr_reviewers (pr_id, reviewer_id) VALUES ($1, $2)",
                2, nullptr, reviewerParams, nullptr, nullptr, 0);
            PQclear(revRes);
        }
    }
    
    return success;
}

bool Database::mergePullRequest(const std::string& prId) {
    const char* params[1] = {prId.c_str()};
    
    PGresult* res = PQexecParams(connection_,
        "UPDATE pull_requests SET status = 'MERGED', merged_at = CURRENT_TIMESTAMP "
        "WHERE id = $1 AND status != 'MERGED'",
        1, nullptr, params, nullptr, nullptr, 0);
    
    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    return success;
}

std::unique_ptr<PullRequest> Database::getPullRequest(const std::string& prId) {
    const char* params[1] = {prId.c_str()};
    
    PGresult* prRes = PQexecParams(connection_,
        "SELECT id, name, author_id, status, created_at, merged_at "
        "FROM pull_requests WHERE id = $1", 1, nullptr, params, nullptr, nullptr, 0);
    
    if (PQresultStatus(prRes) != PGRES_TUPLES_OK || PQntuples(prRes) == 0) {
        PQclear(prRes);
        return nullptr;
    }
    
    auto pr = std::make_unique<PullRequest>(
        PQgetvalue(prRes, 0, 0),
        PQgetvalue(prRes, 0, 1),
        PQgetvalue(prRes, 0, 2),
        PullRequest::stringToStatus(PQgetvalue(prRes, 0, 3))
    );
    
    PGresult* revRes = PQexecParams(connection_,
        "SELECT reviewer_id FROM pr_reviewers WHERE pr_id = $1",
        1, nullptr, params, nullptr, nullptr, 0);
    
    if (PQresultStatus(revRes) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(revRes); i++) {
            pr->assigned_reviewers.push_back(PQgetvalue(revRes, i, 0));
        }
    }
    
    PQclear(prRes);
    PQclear(revRes);
    return pr;
}

bool Database::updatePRReviewers(const std::string& prId, const std::vector<std::string>& reviewers) {
    const char* deleteParams[1] = {prId.c_str()};
    PGresult* deleteRes = PQexecParams(connection_,
        "DELETE FROM pr_reviewers WHERE pr_id = $1",
        1, nullptr, deleteParams, nullptr, nullptr, 0);
    PQclear(deleteRes);
    
    for (const auto& reviewer : reviewers) {
        const char* insertParams[2] = {prId.c_str(), reviewer.c_str()};
        PGresult* insertRes = PQexecParams(connection_,
            "INSERT INTO pr_reviewers (pr_id, reviewer_id) VALUES ($1, $2)",
            2, nullptr, insertParams, nullptr, nullptr, 0);
        PQclear(insertRes);
    }
    
    return true;
}

std::vector<PullRequest> Database::getPRsByReviewer(const std::string& userId) {
    const char* params[1] = {userId.c_str()};
    
    PGresult* res = PQexecParams(connection_,
        "SELECT p.id, p.name, p.author_id, p.status "
        "FROM pull_requests p "
        "JOIN pr_reviewers pr ON p.id = pr.pr_id "
        "WHERE pr.reviewer_id = $1", 1, nullptr, params, nullptr, nullptr, 0);
    
    std::vector<PullRequest> prs;
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            prs.emplace_back(
                PQgetvalue(res, i, 0),
                PQgetvalue(res, i, 1),
                PQgetvalue(res, i, 2),
                PullRequest::stringToStatus(PQgetvalue(res, i, 3))
            );
        }
    }
    
    PQclear(res);
    return prs;
}

bool Database::isPRMerged(const std::string& prId) {
    auto pr = getPullRequest(prId);
    return pr && pr->isMerged();
}

bool Database::prExists(const std::string& prId) {
    const char* params[1] = {prId.c_str()};
    PGresult* res = PQexecParams(connection_,
        "SELECT id FROM pull_requests WHERE id = $1", 1, nullptr, params, nullptr, nullptr, 0);
    
    bool exists = PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0;
    PQclear(res);
    return exists;
}

bool Database::bulkDeactivateUsers(const std::vector<std::string>& userIds) {
    if (userIds.empty()) return true;
    
    PGresult* beginRes = PQexec(connection_, "BEGIN");
    PQclear(beginRes);
    
    try {
        for (const auto& userId : userIds) {
            const char* params[2] = { "false", userId.c_str() };
            PGresult* res = PQexecParams(connection_,
                "UPDATE users SET is_active = $1 WHERE id = $2",
                2, nullptr, params, nullptr, nullptr, 0);
            
            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                PQclear(res);
                throw std::runtime_error("Failed to deactivate user: " + userId);
            }
            PQclear(res);
        }
        
        PGresult* commitRes = PQexec(connection_, "COMMIT");
        bool success = PQresultStatus(commitRes) == PGRES_COMMAND_OK;
        PQclear(commitRes);
        return success;
        
    } catch (const std::exception& e) {
        PGresult* rollbackRes = PQexec(connection_, "ROLLBACK");
        PQclear(rollbackRes);
        return false;
    }
}

std::vector<std::pair<std::string, std::string>> Database::getOpenPRsWithReviewer(const std::string& reviewerId) {
    const char* params[1] = { reviewerId.c_str() };
    PGresult* res = PQexecParams(connection_,
        "SELECT pr.id, pr.name FROM pull_requests pr "
        "JOIN pr_reviewers prr ON pr.id = prr.pr_id "
        "WHERE prr.reviewer_id = $1 AND pr.status = 'OPEN'",
        1, nullptr, params, nullptr, nullptr, 0);
    
    std::vector<std::pair<std::string, std::string>> result;
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            result.emplace_back(
                PQgetvalue(res, i, 0),
                PQgetvalue(res, i, 1)
            );
        }
    }
    PQclear(res);
    return result;
}