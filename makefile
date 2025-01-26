project_name=portage-dev-env

builder-build :
	docker build -f Dockerfile -t $(project_name)-builder:latest --build-arg="USERNAME=${USER}" .

builder-build-clean :
	docker build -f Dockerfile --no-cache -t $(project_name)-builder:latest --build-arg="USERNAME=${USER}" .

builder-run :
	docker run \
		--rm \
		-it \
		--platform linux/amd64 \
		--workdir /workspaces/$(project_name) \
		-v .:/workspaces/$(project_name) \
		$(project_name)-builder:latest \
		/bin/bash
