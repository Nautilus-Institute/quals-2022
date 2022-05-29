CHAL_NAME ?= hash-it-0

build: challenge 


.PHONY:challenge
challenge:
	docker build -f dockers/$(CHAL_NAME)-challenge/Dockerfile -t $(CHAL_NAME)-challenge:latest .

