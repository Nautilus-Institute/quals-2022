CHAL_NAME ?= router
CUR_HASH := $(shell git rev-parse --short HEAD)

build: challenge solver

.PHONY: clean
clean:
	rm -rf $(PWD)/data

.PHONY:challenge
challenge:
	docker build challenge -t $(CHAL_NAME)-challenge:$(CUR_HASH) -t $(CHAL_NAME)-challenge:latest

.PHONY:solver
solver:
	docker build solver -t $(CHAL_NAME)-solver:$(CUR_HASH) -t $(CHAL_NAME)-solver:latest

.PHONY:test
test:
	# build test container that hosts with socat
	docker build challenge -f test/Dockerfile -t $(CHAL_NAME)-test:$(CUR_HASH) -t $(CHAL_NAME)-test:latest
	
	# create docker network if it doesn't already exist
	docker network inspect $(CHAL_NAME)-test >/dev/null 2>&1 || docker network create $(CHAL_NAME)-test
	
	# run challenge
	docker run --rm --network $(CHAL_NAME)-test --name $(CHAL_NAME)-test $(CHAL_NAME)-test:$(CUR_HASH) &
	
	# avoid race condition
	sleep 1
	
	# throw against challenge
	docker run -it --rm --network $(CHAL_NAME)-test -e "HOST=$(CHAL_NAME)-test" -e "PORT=31337" $(CHAL_NAME)-solver:$(CUR_HASH)
	
	# clean up network we created
	docker network rm $(CHAL_NAME)-test
