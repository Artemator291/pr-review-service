#include <iostream>
#include <crow.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "database/Database.h"
#include "services/ReviewAssignmentService.h"

std::string getCurrentTimeISO() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

crow::json::wvalue errorResponse(const std::string& code, const std::string& message) {
    crow::json::wvalue response;
    crow::json::wvalue error;
    error["code"] = code;
    error["message"] = message;
    response["error"] = std::move(error);
    return response;
}

int main() {
    crow::SimpleApp app;
    Database& db = Database::getInstance();
    ReviewAssignmentService assignmentService(db);

    const char* dbUrl = std::getenv("DATABASE_URL");
    if (!dbUrl) {
        dbUrl = "postgresql://user:password@localhost:5432/pr_review_db";
    }

    if (!db.connect(dbUrl)) {
        std::cerr << "Failed to connect to database" << std::endl;
        return 1;
    }

    CROW_ROUTE(app, "/health")([](){
        crow::json::wvalue response;
        response["status"] = "OK";
        return response;
    });

    CROW_ROUTE(app, "/team/add").methods("POST"_method)([&db](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) return crow::response(400, errorResponse("BAD_REQUEST", "Invalid JSON"));

        std::string teamName = json["team_name"].s();
        if (db.teamExists(teamName)) {
            return crow::response(400, errorResponse("TEAM_EXISTS", "team_name already exists"));
        }

        Team team(teamName);
        auto members = json["members"];
        for (size_t i = 0; i < members.size(); i++) {
            team.members.emplace_back(
                members[i]["user_id"].s(),
                members[i]["username"].s(),
                teamName,
                members[i]["is_active"].b()
            );
        }

        if (!db.createTeam(team)) {
            return crow::response(500, errorResponse("INTERNAL_ERROR", "Failed to create team"));
        }

        auto createdTeam = db.getTeam(teamName);
        if (!createdTeam) {
            return crow::response(500, errorResponse("INTERNAL_ERROR", "Failed to retrieve created team"));
        }

        crow::json::wvalue response;
        response["team"]["team_name"] = createdTeam->name;
        
        crow::json::wvalue membersJson;
        int i = 0;
        for (const auto& member : createdTeam->members) {
            crow::json::wvalue m;
            m["user_id"] = member.id;
            m["username"] = member.username;
            m["is_active"] = member.is_active;
            membersJson[i++] = m;
        }
        response["team"]["members"] = membersJson;

        return crow::response(201, response);
    });

    CROW_ROUTE(app, "/team/get").methods("GET"_method)([&db](const crow::request& req) {
        std::string teamName = req.url_params.get("team_name");
        if (teamName.empty()) {
            return crow::response(400, errorResponse("BAD_REQUEST", "team_name parameter is required"));
        }

        auto team = db.getTeam(teamName);
        if (!team) {
            return crow::response(404, errorResponse("NOT_FOUND", "Team not found"));
        }

        crow::json::wvalue response;
        response["team_name"] = team->name;
        
        crow::json::wvalue membersJson;
        int i = 0;
        for (const auto& member : team->members) {
            crow::json::wvalue m;
            m["user_id"] = member.id;
            m["username"] = member.username;
            m["is_active"] = member.is_active;
            membersJson[i++] = m;
        }
        response["members"] = membersJson;

        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/users/setIsActive").methods("POST"_method)([&db](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) return crow::response(400, errorResponse("BAD_REQUEST", "Invalid JSON"));

        std::string userId = json["user_id"].s();
        bool isActive = json["is_active"].b();

        auto user = db.getUser(userId);
        if (!user) {
            return crow::response(404, errorResponse("NOT_FOUND", "User not found"));
        }

        if (!db.setUserActive(userId, isActive)) {
            return crow::response(500, errorResponse("INTERNAL_ERROR", "Failed to update user"));
        }

        user = db.getUser(userId);
        crow::json::wvalue response;
        response["user"]["user_id"] = user->id;
        response["user"]["username"] = user->username;
        response["user"]["team_name"] = user->team_name;
        response["user"]["is_active"] = user->is_active;

        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/users/getReview").methods("GET"_method)([&db](const crow::request& req) {
        std::string userId = req.url_params.get("user_id");
        if (userId.empty()) {
            return crow::response(400, errorResponse("BAD_REQUEST", "user_id parameter is required"));
        }

        auto user = db.getUser(userId);
        if (!user) {
            return crow::response(404, errorResponse("NOT_FOUND", "User not found"));
        }

        auto prs = db.getPRsByReviewer(userId);
        
        crow::json::wvalue response;
        response["user_id"] = userId;
        
        crow::json::wvalue prsJson;
        int i = 0;
        for (const auto& pr : prs) {
            crow::json::wvalue p;
            p["pull_request_id"] = pr.id;
            p["pull_request_name"] = pr.name;
            p["author_id"] = pr.author_id;
            p["status"] = pr.getStatusString();
            prsJson[i++] = p;
        }
        response["pull_requests"] = prsJson;

        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/pullRequest/create").methods("POST"_method)([&db, &assignmentService](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) return crow::response(400, errorResponse("BAD_REQUEST", "Invalid JSON"));

        std::string prId = json["pull_request_id"].s();
        std::string prName = json["pull_request_name"].s();
        std::string authorId = json["author_id"].s();

        if (db.prExists(prId)) {
            return crow::response(409, errorResponse("PR_EXISTS", "PR id already exists"));
        }

        auto author = db.getUser(authorId);
        if (!author) {
            return crow::response(404, errorResponse("NOT_FOUND", "Author not found"));
        }

        auto reviewers = assignmentService.assignReviewers(authorId, author->team_name);
        PullRequest pr(prId, prName, authorId);
        pr.assigned_reviewers = reviewers;

        if (!db.createPullRequest(pr)) {
            return crow::response(500, errorResponse("INTERNAL_ERROR", "Failed to create PR"));
        }

        auto createdPR = db.getPullRequest(prId);
        if (!createdPR) {
            return crow::response(500, errorResponse("INTERNAL_ERROR", "Failed to retrieve created PR"));
        }

        crow::json::wvalue response;
        response["pr"]["pull_request_id"] = createdPR->id;
        response["pr"]["pull_request_name"] = createdPR->name;
        response["pr"]["author_id"] = createdPR->author_id;
        response["pr"]["status"] = createdPR->getStatusString();
        response["pr"]["createdAt"] = getCurrentTimeISO();
        
        crow::json::wvalue reviewersJson;
        int i = 0;
        for (const auto& reviewer : createdPR->assigned_reviewers) {
            reviewersJson[i++] = reviewer;
        }
        response["pr"]["assigned_reviewers"] = reviewersJson;

        return crow::response(201, response);
    });

    CROW_ROUTE(app, "/pullRequest/merge").methods("POST"_method)([&db](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) return crow::response(400, errorResponse("BAD_REQUEST", "Invalid JSON"));

        std::string prId = json["pull_request_id"].s();

        auto pr = db.getPullRequest(prId);
        if (!pr) {
            return crow::response(404, errorResponse("NOT_FOUND", "PR not found"));
        }

        if (!db.mergePullRequest(prId)) {
            return crow::response(500, errorResponse("INTERNAL_ERROR", "Failed to merge PR"));
        }

        pr = db.getPullRequest(prId);
        
        crow::json::wvalue response;
        response["pr"]["pull_request_id"] = pr->id;
        response["pr"]["pull_request_name"] = pr->name;
        response["pr"]["author_id"] = pr->author_id;
        response["pr"]["status"] = pr->getStatusString();
        response["pr"]["mergedAt"] = getCurrentTimeISO();
        
        crow::json::wvalue reviewersJson;
        int i = 0;
        for (const auto& reviewer : pr->assigned_reviewers) {
            reviewersJson[i++] = reviewer;
        }
        response["pr"]["assigned_reviewers"] = reviewersJson;

        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/pullRequest/reassign").methods("POST"_method)([&db, &assignmentService](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json) return crow::response(400, errorResponse("BAD_REQUEST", "Invalid JSON"));

        std::string prId = json["pull_request_id"].s();
        std::string oldReviewerId = json["old_user_id"].s();

        try {
            std::string newReviewerId = assignmentService.reassignReviewer(prId, oldReviewerId);
            
            auto pr = db.getPullRequest(prId);
            if (!pr) {
                return crow::response(404, errorResponse("NOT_FOUND", "PR not found"));
            }

            auto& reviewers = pr->assigned_reviewers;
            auto it = std::find(reviewers.begin(), reviewers.end(), oldReviewerId);
            if (it != reviewers.end()) {
                *it = newReviewerId;
            }

            if (!db.updatePRReviewers(prId, reviewers)) {
                return crow::response(500, errorResponse("INTERNAL_ERROR", "Failed to update reviewers"));
            }

            pr = db.getPullRequest(prId);
            
            crow::json::wvalue response;
            response["pr"]["pull_request_id"] = pr->id;
            response["pr"]["pull_request_name"] = pr->name;
            response["pr"]["author_id"] = pr->author_id;
            response["pr"]["status"] = pr->getStatusString();
            
            crow::json::wvalue reviewersJson;
            int i = 0;
            for (const auto& reviewer : pr->assigned_reviewers) {
                reviewersJson[i++] = reviewer;
            }
            response["pr"]["assigned_reviewers"] = reviewersJson;
            response["replaced_by"] = newReviewerId;

            return crow::response(200, response);

        } catch (const std::runtime_error& e) {
            std::string errorMsg = e.what();
            if (errorMsg == "Cannot reassign reviewers for merged PR") {
                return crow::response(409, errorResponse("PR_MERGED", "cannot reassign on merged PR"));
            } else if (errorMsg == "Reviewer not found") {
                return crow::response(404, errorResponse("NOT_FOUND", "Reviewer not found"));
            } else if (errorMsg == "PR not found") {
                return crow::response(404, errorResponse("NOT_FOUND", "PR not found"));
            } else if (errorMsg == "Reviewer is not assigned to this PR") {
                return crow::response(409, errorResponse("NOT_ASSIGNED", "reviewer is not assigned to this PR"));
            } else if (errorMsg == "No active replacement candidate in team") {
                return crow::response(409, errorResponse("NO_CANDIDATE", "no active replacement candidate in team"));
            } else {
                return crow::response(500, errorResponse("INTERNAL_ERROR", errorMsg));
            }
        }
    });

    CROW_ROUTE(app, "/stats/review-assignments").methods("GET"_method)([&db]() {
        crow::json::wvalue response;
        response["message"] = "Statistics endpoint - to be implemented";
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/stats/review-assignments").methods("GET"_method)([&db]() {
    try {
        const char* totalPRsQuery = "SELECT COUNT(*) FROM pull_requests";
        PGresult* totalRes = PQexec(db.connection_, totalPRsQuery);
        int totalPRs = 0;
        if (PQresultStatus(totalRes) == PGRES_TUPLES_OK && PQntuples(totalRes) > 0) {
            totalPRs = std::stoi(PQgetvalue(totalRes, 0, 0));
        }
        PQclear(totalRes);

        const char* openPRsQuery = "SELECT COUNT(*) FROM pull_requests WHERE status = 'OPEN'";
        PGresult* openRes = PQexec(db.connection_, openPRsQuery);
        int openPRs = 0;
        if (PQresultStatus(openRes) == PGRES_TUPLES_OK && PQntuples(openRes) > 0) {
            openPRs = std::stoi(PQgetvalue(openRes, 0, 0));
        }
        PQclear(openRes);

        const char* mergedPRsQuery = "SELECT COUNT(*) FROM pull_requests WHERE status = 'MERGED'";
        PGresult* mergedRes = PQexec(db.connection_, mergedPRsQuery);
        int mergedPRs = 0;
        if (PQresultStatus(mergedRes) == PGRES_TUPLES_OK && PQntuples(mergedRes) > 0) {
            mergedPRs = std::stoi(PQgetvalue(mergedRes, 0, 0));
        }
        PQclear(mergedRes);

        const char* userStatsQuery = 
            "SELECT u.id, u.username, COUNT(pr.reviewer_id) as assignment_count "
            "FROM users u LEFT JOIN pr_reviewers pr ON u.id = pr.reviewer_id "
            "WHERE u.is_active = true "
            "GROUP BY u.id, u.username "
            "ORDER BY assignment_count DESC";
        
        PGresult* userRes = PQexec(db.connection_, userStatsQuery);
        
        crow::json::wvalue response;
        response["summary"]["total_prs"] = totalPRs;
        response["summary"]["open_prs"] = openPRs;
        response["summary"]["merged_prs"] = mergedPRs;
        response["summary"]["total_assignments"] = totalPRs * 2;

        if (PQresultStatus(userRes) == PGRES_TUPLES_OK) {
            crow::json::wvalue userStats;
            for (int i = 0; i < PQntuples(userRes); i++) {
                crow::json::wvalue userStat;
                userStat["user_id"] = PQgetvalue(userRes, i, 0);
                userStat["username"] = PQgetvalue(userRes, i, 1);
                userStat["assignment_count"] = std::stoi(PQgetvalue(userRes, i, 2));
                userStats[i] = userStat;
            }
            response["user_assignments"] = userStats;
        }
        PQclear(userRes);

        const char* prStatsQuery = 
            "SELECT p.id, p.name, p.status, COUNT(pr.reviewer_id) as reviewer_count "
            "FROM pull_requests p LEFT JOIN pr_reviewers pr ON p.id = pr.pr_id "
            "GROUP BY p.id, p.name, p.status "
            "ORDER BY p.created_at DESC";
        
        PGresult* prRes = PQexec(db.connection_, prStatsQuery);
        
        if (PQresultStatus(prRes) == PGRES_TUPLES_OK) {
            crow::json::wvalue prStats;
            for (int i = 0; i < PQntuples(prRes); i++) {
                crow::json::wvalue prStat;
                prStat["pr_id"] = PQgetvalue(prRes, i, 0);
                prStat["name"] = PQgetvalue(prRes, i, 1);
                prStat["status"] = PQgetvalue(prRes, i, 2);
                prStat["reviewer_count"] = std::stoi(PQgetvalue(prRes, i, 3));
                prStats[i] = prStat;
            }
            response["pr_assignments"] = prStats;
        }
        PQclear(prRes);

        return crow::response(200, response);

    } catch (const std::exception& e) {
        return crow::response(500, errorResponse("INTERNAL_ERROR", "Failed to generate statistics"));
    }
    });

    CROW_ROUTE(app, "/users/bulk-deactivate").methods("POST"_method)([&db, &assignmentService](const crow::request& req) {
    auto start = std::chrono::high_resolution_clock::now();
    
    auto json = crow::json::load(req.body);
    if (!json) return crow::response(400, errorResponse("BAD_REQUEST", "Invalid JSON"));

    auto userIdsJson = json["user_ids"];
    std::vector<std::string> userIds;
    for (size_t i = 0; i < userIdsJson.size(); i++) {
        userIds.push_back(userIdsJson[i].s());
    }

    bool reassignOpenPRs = json["reassign_open_prs"].b();

    for (const auto& userId : userIds) {
        if (!db.getUser(userId)) {
            return crow::response(404, errorResponse("NOT_FOUND", "User not found: " + userId));
        }
    }

    if (reassignOpenPRs) {
        for (const auto& userId : userIds) {
            auto openPRs = db.getOpenPRsWithReviewer(userId);
            for (const auto& [prId, prName] : openPRs) {
                try {
                    assignmentService.reassignReviewer(prId, userId);
                } catch (const std::exception& e) {
                    std::cerr << "Warning: Failed to reassign PR " << prId 
                              << " from user " << userId << ": " << e.what() << std::endl;
                }
            }
        }
    }

    if (!db.bulkDeactivateUsers(userIds)) {
        return crow::response(500, errorResponse("INTERNAL_ERROR", "Failed to deactivate users"));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    crow::json::wvalue response;
    response["deactivated_users"] = userIds.size();
    response["reassign_open_prs"] = reassignOpenPRs;
    response["processing_time_ms"] = duration.count();
    response["status"] = "success";

    return crow::response(200, response);
    });

    std::cout << "PR Review Service starting on http://localhost:8080" << std::endl;
    std::cout << "Health check: http://localhost:8080/health" << std::endl;
    
    app.port(8080).multithreaded().run();
    
    db.disconnect();
    return 0;
}