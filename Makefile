.PHONY: build run clean docker-up docker-down test

build:
	mkdir -p build
	cd build && cmake .. && make

run: build
	./build/pr_review_service

clean:
	rm -rf build

docker-up:
	docker-compose up --build

docker-down:
	docker-compose down

test: build
	./build/pr_review_service &
	sleep 2
	curl http://localhost:8080/health
	curl http://localhost:8080/
	curl http://localhost:8080/api/test
	pkill pr_review_service

format:
	find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i