for /f "delims=" %%i in ('git rev-parse --abbrev-ref HEAD') do set BRANCH=%%i

if not exist client git clone -b %BRANCH% https://github.com/uroni/urbackup_frontend_wx client

cd client

git fetch
git checkout -b %BRANCH% origin/%BRANCH%
git checkout %BRANCH%
git reset --hard
git pull
