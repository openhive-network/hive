"""Async DB adapter for hivemind API."""

import logging
from time import perf_counter as perf

import sqlalchemy
from sqlalchemy.engine.url import make_url
from aiopg.sa import create_engine

from hive.utils.stats import Stats

logging.getLogger('sqlalchemy.engine').setLevel(logging.WARNING)
log = logging.getLogger(__name__)

def sqltimer(function):
    """Decorator for DB query methods which tracks timing."""
    async def _wrapper(*args, **kwargs):
        start = perf()
        result = await function(*args, **kwargs)
        Stats.log_db(args[1], perf() - start)
        return result
    return _wrapper

class Db:
    """Wrapper for aiopg.sa db driver."""

    @classmethod
    async def create(cls, url, maxsize):
        """Factory method."""
        instance = Db()
        await instance.init(url, maxsize)
        return instance

    def __init__(self):
        self.db = None
        self._prep_sql = {}

    async def init(self, url, maxsize):
        """Initialize the aiopg.sa engine."""
        conf = make_url(url)
        dsn = {}
        if conf.username:
            dsn['user'] = conf.username
        if conf.database:
            dsn['database'] = conf.database
        if conf.password:
            dsn['password'] = conf.password
        if conf.host:
            dsn['host'] = conf.host
        if conf.port:
            dsn['port'] = conf.port
        if 'application_name' not in conf.query:
            dsn['application_name'] = 'hive_server'
        self.db = await create_engine(**dsn, maxsize=maxsize, **conf.query)

    def close(self):
        """Close pool."""
        self.db.close()

    async def wait_closed(self):
        """Wait for releasing and closing all acquired connections."""
        await self.db.wait_closed()

    @sqltimer
    async def query_all(self, sql, **kwargs):
        """Perform a `SELECT n*m`"""
        async with self.db.acquire() as conn:
            cur = await self._query(conn, sql, **kwargs)
            res = await cur.fetchall()
        return res

    @sqltimer
    async def query_row(self, sql, **kwargs):
        """Perform a `SELECT 1*m`"""
        async with self.db.acquire() as conn:
            cur = await self._query(conn, sql, **kwargs)
            res = await cur.first()
        return res

    @sqltimer
    async def query_col(self, sql, **kwargs):
        """Perform a `SELECT n*1`"""
        async with self.db.acquire() as conn:
            cur = await self._query(conn, sql, **kwargs)
            res = await cur.fetchall()
        return [r[0] for r in res]

    @sqltimer
    async def query_one(self, sql, **kwargs):
        """Perform a `SELECT 1*1`"""
        async with self.db.acquire() as conn:
            cur = await self._query(conn, sql, **kwargs)
            row = await cur.first()
        return row[0] if row else None

    @sqltimer
    async def query(self, sql, **kwargs):
        """Perform a write query"""
        async with self.db.acquire() as conn:
            await self._query(conn, sql, **kwargs)

    async def _query(self, conn, sql, **kwargs):
        """Send a query off to SQLAlchemy."""
        try:
            return await conn.execute(self._sql_text(sql), **kwargs)
        except Exception as e:
            log.warning("[SQL-ERR] %s in query %s (%s)",
                        e.__class__.__name__, sql, kwargs)
            raise e

    def _sql_text(self, sql):
        if sql in self._prep_sql:
            query = self._prep_sql[sql]
        else:
            query = sqlalchemy.text(sql).execution_options(autocommit=False)
            self._prep_sql[sql] = query
        return query
