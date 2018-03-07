#!/bin/bash
set -eu

scripts="$(dirname "$0")"

docker login -u papercurrency -p "$DOCKER_PASSWORD"

# We push this just so it can be a cache next time
"$scripts"/custom-timeout.sh 30 docker push papercurrency/paper-ci

tags=()
if [ -n "$TRAVIS_BRANCH" ]; then
    tags+=("$TRAVIS_BRANCH")
elif [ -n "$TRAVIS_TAG" ]; then
    tags+=("$TRAVIS_TAG" latest)
fi

ci/build-docker-image.sh docker/node/Dockerfile papercurrency/paper
for tag in "${tags[@]}"; do
    # Sanitize docker tag
    # https://docs.docker.com/engine/reference/commandline/tag/
    tag="$(printf '%s' "$tag" | tr -c '[a-z][A-Z][0-9]_.-' -)"
    if [ "$tag" != "latest" ]; then
        docker tag papercurrency/paper papercurrency/paper:"$tag"
    fi
    "$scripts"/custom-timeout.sh 30 docker push papercurrency/paper:"$tag"
done
