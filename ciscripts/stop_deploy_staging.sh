ID=`cat $1`
screen -S hived-deploy-$CI_ENVIRONMENT_NAME-$ID -X quit