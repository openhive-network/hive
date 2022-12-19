variable "REGISTRY" {} #CI_REGISTRY_IMAGE
#variable "CI_IMAGE_TAG" {}
variable "BUILD_IMAGE_TAG" {}
variable "BUILD_HIVE_TESTNET" {}
variable "HIVE_CONVERTER_BUILD" {}
#variable "HIVE_LINT" {}
#variable "P2P_PORT" {}
#variable "WS_PORT" {}
#variable "HTTP_PORT" {}
#variable "CLI_WALLET_PORT" {}
variable "CACHE_REF" {}
variable "IMAGE_TAG_PREFIX" {}

target base_instance-ci {
    args = {
        CI_REGISTRY_IMAGE=REGISTRY
        BUILD_HIVE_TESTNET=BUILD_HIVE_TESTNET
        HIVE_CONVERTER_BUILD=HIVE_CONVERTER_BUILD
        BUILD_IMAGE_TAG=BUILD_IMAGE_TAG
    }
    cache-from = [ "type=local,src=${CACHE_REF}" ]
    cache-to = [ "type=local,mode=max,dest=${CACHE_REF}" ]
    tags = [ "${REGISTRY}base_instance:base_instance-${BUILD_IMAGE_TAG}" ] 
    target = "base_instance"
}

target instance-ci {
    args = {
        CI_REGISTRY_IMAGE=REGISTRY
        BUILD_HIVE_TESTNET=BUILD_HIVE_TESTNET
        HIVE_CONVERTER_BUILD=HIVE_CONVERTER_BUILD
        BUILD_IMAGE_TAG=BUILD_IMAGE_TAG
    }
    cache-from = [ "type=local,src=${CACHE_REF}" ]
    cache-to = [ "type=local,mode=max,dest=${CACHE_REF}" ]
    contexts = {
        "${REGISTRY}base_instance:base_instance-${BUILD_IMAGE_TAG}" = "target:base_instance-ci"
    }
    tags = [ "${REGISTRY}${IMAGE_TAG_PREFIX}instance:instance-${BUILD_IMAGE_TAG}" ]
    target = "instance"
}

target data-ci {
    args = {
        CI_REGISTRY_IMAGE=REGISTRY
        BUILD_HIVE_TESTNET=BUILD_HIVE_TESTNET
        HIVE_CONVERTER_BUILD=HIVE_CONVERTER_BUILD
        BUILD_IMAGE_TAG=BUILD_IMAGE_TAG
    }
    # Cache disabled - takes too much space due to replay data
    #cache-from = [ "type=local,src=${CACHE_REF}" ]
    #cache-to = [ "type=local,mode=max,dest=${CACHE_REF}" ]
    contexts = {
        "${REGISTRY}base_instance:base_instance-${BUILD_IMAGE_TAG}" = "target:base_instance-ci"
    }
    tags = [ "${REGISTRY}data:data-${BUILD_IMAGE_TAG}" ]
    target = "data"
}