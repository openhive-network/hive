#! /bin/bash


echo "Build Dockerfile"

cat <<EOF > Dockerfile
FROM scratch
COPY block_log_open_sign /block_log_open_sign
COPY block_log_single_sign /block_log_single_sign
COPY block_log_multi_sign /block_log_multi_sign
EOF

cat Dockerfile

echo "Build docker image containing universal_block_logs"

echo "Image name $CI_DOCKER_REGISTRY/hive/hive/universal-block-logs:$CI_COMMIT_SHORT_SHA"
docker build -t $CI_DOCKER_REGISTRY/hive/hive/universal-block-logs:$CI_COMMIT_SHORT_SHA .

echo "Image name $CI_DOCKER_REGISTRY/hive/hive/universal-block-logs:latest"
docker build -t $CI_DOCKER_REGISTRY/hive/hive/universal-block-logs:latest .

docker login -u "$CI_REGISTRY_USER" --password-stdin "$CI_REGISTRY" <<< "$CI_REGISTRY_PASSWORD"

docker push $CI_DOCKER_REGISTRY/hive/hive/universal-block-logs:$CI_COMMIT_SHORT_SHA
docker push $CI_DOCKER_REGISTRY/hive/hive/universal-block-logs:latest

echo "Created and push docker image with universal block logs"
