#include <iostream>
#include <cassert>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <curl/curl.h>

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t totalSize = size * nmemb;
    response->append((char*)contents, totalSize);
    return totalSize;
}

bool makeRequest(const std::string& url, const std::string& method = "GET", 
                 const std::string& data = "", int expectedStatus = 200) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string response;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (!data.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        }
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    bool success = (res == CURLE_OK) && (response_code == expectedStatus);
    if (!success) {
        std::cout << "Test failed: " << url << " Status: " << response_code 
                  << " Response: " << response << std::endl;
    }
    return success;
}

void runIntegrationTests() {
    std::cout << "Starting integration tests...\n";

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Test 1: Health check
    assert(makeRequest("http://localhost:8080/health"));
    std::cout << "Health check passed\n";

    // Test 2: Create team
    std::string teamData = R"({
        "team_name": "test-team",
        "members": [
            {"user_id": "test-user-1", "username": "Test User 1", "is_active": true},
            {"user_id": "test-user-2", "username": "Test User 2", "is_active": true},
            {"user_id": "test-user-3", "username": "Test User 3", "is_active": true}
        ]
    })";
    assert(makeRequest("http://localhost:8080/team/add", "POST", teamData, 201));
    std::cout << "Team creation passed\n";

    // Test 3: Create PR
    std::string prData = R"({
        "pull_request_id": "test-pr-1",
        "pull_request_name": "Test PR",
        "author_id": "test-user-1"
    })";
    assert(makeRequest("http://localhost:8080/pullRequest/create", "POST", prData, 201));
    std::cout << "PR creation passed\n";

    // Test 4: Get user reviews
    assert(makeRequest("http://localhost:8080/users/getReview?user_id=test-user-2"));
    std::cout << "Get user reviews passed\n";

    // Test 5: Merge PR
    std::string mergeData = R"({
        "pull_request_id": "test-pr-1"
    })";
    assert(makeRequest("http://localhost:8080/pullRequest/merge", "POST", mergeData, 200));
    std::cout << "PR merge passed\n";

    // Test 6: Statistics
    assert(makeRequest("http://localhost:8080/stats/review-assignments"));
    std::cout << "Statistics endpoint passed\n";

    std::cout << "All integration tests passed!\n";
}

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    std::thread serverThread([]() {
        system("./pr_review_service");
    });

    std::this_thread::sleep_for(std::chrono::seconds(3));

    try {
        runIntegrationTests();
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        curl_global_cleanup();
        return 1;
    }

    curl_global_cleanup();
    return 0;
}