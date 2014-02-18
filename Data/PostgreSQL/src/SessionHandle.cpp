//
// SessionHandle.cpp
//
// $Id: //poco/1.5/Data/PostgreSQL/src/SessionHandle.cpp#1 $
//
// Library: Data
// Package: PostgreSQL
// Module:  SessionHandle
//
// Copyright (c) 2008, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
//
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//


#include "Poco/Data/PostgreSQL/SessionHandle.h"
#include "Poco/Data/PostgreSQL/PostgreSQLException.h"
#include "Poco/Data/PostgreSQL/PostgreSQLTypes.h"
#include "Poco/Data/Session.h"
#include "Poco/NumberFormatter.h"


#define POCO_POSTGRESQL_VERSION_NUMBER ((NDB_VERSION_MAJOR<<16) | (NDB_VERSION_MINOR<<8) | (NDB_VERSION_BUILD&0xFF))

namespace Poco {
namespace Data {
namespace PostgreSQL {

//const std::string SessionHandle::POSTGRESQL_READ_UNCOMMITTED = "READ UNCOMMITTED";
const std::string SessionHandle::POSTGRESQL_READ_COMMITTED  = "READ COMMITTED";
const std::string SessionHandle::POSTGRESQL_REPEATABLE_READ = "REPEATABLE READ";
const std::string SessionHandle::POSTGRESQL_SERIALIZABLE    = "SERIALIZABLE";


SessionHandle::SessionHandle()
    : _pConnection              ( 0 ),
      _isAutoCommit             ( true ),
      _isAsynchronousCommit     ( false ),
      _tranactionIsolationLevel ( Session::TRANSACTION_READ_COMMITTED )
{
}

SessionHandle::~SessionHandle()
{
    try
    {
        disconnect();
    }
    catch (...)
    {
    }
}

void
SessionHandle::connect ( const std::string & aConnectionString )
{
    if ( isConnected() )
    {
		throw ConnectionFailedException( "Already Connected" );
    }
    
	{
        Poco::FastMutex::ScopedLock mutexLocker( _sessionMutex );
    
        _pConnection = PQconnectdb( aConnectionString.c_str() );
    }
    
    if ( ! isConnected() )
    {
		throw ConnectionFailedException( std::string( "Connection Error: " ) + lastError() );
    }
    
    _connectionString = aConnectionString;
}
    
void
SessionHandle::connect ( const char* aConnectionString )
{
    connect( std::string( aConnectionString ) );
}
    
void
SessionHandle::connect(const char* aHost, const char* aUser, const char* aPassword, const char* aDatabase, unsigned short aPort, unsigned int aConnectionTimeout )
{
    std::string connectionString;
    
    connectionString.append( "host=" );
    connectionString.append( aHost );
    connectionString.append( " ");

    connectionString.append( "user=" );
    connectionString.append( aUser );
    connectionString.append( " ");

    connectionString.append( "password=" );
    connectionString.append( aPassword );
    connectionString.append( " ");

    connectionString.append( "dbname=" );
    connectionString.append( aDatabase );
    connectionString.append( " ");
    
    connectionString.append( "port=" );
    Poco::NumberFormatter::append( connectionString, aPort );
    connectionString.append( " ");
    
    connectionString.append( "connect_timeout=" );
    Poco::NumberFormatter::append( connectionString, aConnectionTimeout );

    connect( connectionString );
}

bool
SessionHandle::isConnected() const
{
	Poco::FastMutex::ScopedLock mutexLocker( _sessionMutex );

    if (    _pConnection
         && PQstatus( _pConnection ) == CONNECTION_OK )
    {
        return true;
    }
    
    return false;
}

void
SessionHandle::disconnect()
{
    if ( _pConnection )
    {
        Poco::FastMutex::ScopedLock mutexLocker( _sessionMutex );

        PQfinish( _pConnection );
        _pConnection = 0;
    }
    
    _connectionString = std::string();
}

bool
SessionHandle::reset()
{
    if ( _pConnection )
    {
        {
            Poco::FastMutex::ScopedLock mutexLocker( _sessionMutex );
    
            PQreset( _pConnection );
        }
        
        if ( isConnected() )
        {
            return true;
        }
    }
    
    return false;
}
    
std::string
SessionHandle::lastError() const
{
    if ( ! isConnected() )
    {
		return std::string();
    }

	Poco::FastMutex::ScopedLock mutexLocker( _sessionMutex );
    
    std::string lastError (  0 != _pConnection ? PQerrorMessage( _pConnection ) : "not connected" );
    
    return lastError;
}

void SessionHandle::startTransaction()
{
    if ( ! isConnected() )
    {
		throw NotConnectedException();
    }

	PGresult * pPQResult = 0;
    
    {
        Poco::FastMutex::ScopedLock mutexLocker( _sessionMutex );
    
        pPQResult = PQexec( _pConnection, "BEGIN" );
    }
    
    PQResultClear resultClearer( pPQResult );
    
    if ( PQresultStatus( pPQResult ) != PGRES_COMMAND_OK )
    {
		throw StatementException( std::string( "BEGIN statement failed:: " ) + lastError() );
    }
}


void SessionHandle::commit()
{
    if ( ! isConnected() )
    {
		throw NotConnectedException();
    }

	PGresult * pPQResult = 0;
    
    {
        Poco::FastMutex::ScopedLock mutexLocker( _sessionMutex );
        
        pPQResult = PQexec( _pConnection, "COMMIT" );
    }
    
    PQResultClear resultClearer( pPQResult );
    
    if ( PQresultStatus( pPQResult ) != PGRES_COMMAND_OK )
    {
		throw StatementException( std::string( "COMMIT statement failed:: " ) + lastError() );
    }
}


void SessionHandle::rollback()
{
    if ( ! isConnected() )
    {
		throw NotConnectedException();
    }

	PGresult * pPQResult = 0;
    
    {
        Poco::FastMutex::ScopedLock mutexLocker( _sessionMutex );
    
        pPQResult = PQexec( _pConnection, "ROLLBACK" );
    }
    
    PQResultClear resultClearer( pPQResult );
    
    if ( PQresultStatus( pPQResult ) != PGRES_COMMAND_OK )
    {
		throw StatementException( std::string( "ROLLBACK statement failed:: " ) + lastError() );
    }
}

void
SessionHandle::setAutoCommit( bool aShouldAutoCommit )
{
    if ( aShouldAutoCommit == _isAutoCommit )
    {
        return;
    }
    
    if ( aShouldAutoCommit ) {
        commit();  // end any in process transaction
    } else {
        startTransaction();  // start a new transaction
    }
    
    _isAutoCommit = aShouldAutoCommit;
}

void
SessionHandle::setAsynchronousCommit( bool aShouldAsynchronousCommit )
{
    if ( ! isConnected() )
    {
		throw NotConnectedException();
    }

    if ( aShouldAsynchronousCommit == _isAsynchronousCommit )
    {
        return;
    }

	PGresult * pPQResult = 0;
    
    {
        Poco::FastMutex::ScopedLock mutexLocker( _sessionMutex );
        
        pPQResult = PQexec( _pConnection, aShouldAsynchronousCommit ? "SET SYNCHRONOUS COMMIT TO OFF" : "SET SYNCHRONOUS COMMIT TO ON" );
    }
    
    PQResultClear resultClearer( pPQResult );
    
    if ( PQresultStatus( pPQResult ) != PGRES_COMMAND_OK )
    {
		throw StatementException( std::string( "SET SYNCHRONUS COMMIT statement failed:: " ) + lastError() );
    }

    _isAsynchronousCommit = aShouldAsynchronousCommit;
    
}

void
SessionHandle::cancel()
{
    if ( ! isConnected() )
    {
		throw NotConnectedException();
    }

    PGcancel* ptrPGCancel = 0;
    
    {
        Poco::FastMutex::ScopedLock mutexLocker( _sessionMutex );
    
        ptrPGCancel = PQgetCancel( _pConnection );
    }
    
    PGCancelFree cancelFreer( ptrPGCancel );
    
    PQcancel( ptrPGCancel, 0, 0 ); // no error buffer
}

void
SessionHandle::setTransactionIsolation( Poco::UInt32 aTI )
{
    if ( ! isConnected() )
    {
		throw NotConnectedException();
    }

    if ( aTI == _tranactionIsolationLevel )
    {
        return;
    }

    if ( ! hasTransactionIsolation( aTI ) )
    {
		throw Poco::InvalidArgumentException( "setTransactionIsolation()" );
    }

    std::string isolationLevel;

	switch ( aTI )
	{
	case Session::TRANSACTION_READ_COMMITTED:
		isolationLevel = POSTGRESQL_READ_COMMITTED; break;
	case Session::TRANSACTION_REPEATABLE_READ:
		isolationLevel = POSTGRESQL_REPEATABLE_READ; break;
	case Session::TRANSACTION_SERIALIZABLE:
		isolationLevel = POSTGRESQL_SERIALIZABLE; break;
	}
    
    PGresult* pPQResult = 0;
    
	{
        Poco::FastMutex::ScopedLock mutexLocker( _sessionMutex );

        pPQResult = PQexec( _pConnection, Poco::format( "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL %s", isolationLevel).c_str() );
    }
    
    PQResultClear resultClearer( pPQResult );
    
    if ( PQresultStatus( pPQResult ) != PGRES_COMMAND_OK )
    {
		throw StatementException( std::string( "set transaction isolation statement failed: " ) + lastError() );
    }
    
    _tranactionIsolationLevel = aTI;
}


Poco::UInt32
SessionHandle::transactionIsolation()
{
    return _tranactionIsolationLevel;
}


bool
SessionHandle::hasTransactionIsolation( Poco::UInt32 aTI )
{
	return    Session::TRANSACTION_READ_COMMITTED   == aTI
           || Session::TRANSACTION_REPEATABLE_READ  == aTI
           || Session::TRANSACTION_SERIALIZABLE     == aTI;
}

int
SessionHandle::serverVersion() const
{
    if ( ! isConnected() )
    {
		throw NotConnectedException();
    }
    
	Poco::FastMutex::ScopedLock mutexLocker( _sessionMutex );
    
    return PQserverVersion( _pConnection );
}

int
SessionHandle::serverProcessID() const
{
    if ( ! isConnected() )
    {
		throw NotConnectedException();
    }
    
	Poco::FastMutex::ScopedLock mutexLocker( _sessionMutex );
    
    return PQbackendPID( _pConnection );
}
    
int
SessionHandle::protocoVersion() const
{
    if ( ! isConnected() )
    {
		throw NotConnectedException();
    }
    
	Poco::FastMutex::ScopedLock mutexLocker( _sessionMutex );
    
    return PQprotocolVersion( _pConnection );
}

std::string
SessionHandle::clientEncoding() const
{
    if ( ! isConnected() )
    {
		throw NotConnectedException();
    }

    Poco::FastMutex::ScopedLock mutexLocker( _sessionMutex );
    
    return pg_encoding_to_char( PQclientEncoding( _pConnection ) );
}

int
SessionHandle::libpqVersion() const
{
    return PQlibVersion();
}


SessionParametersMap
SessionHandle::setConnectionInfoParameters( PQconninfoOption * aConnectionInfoOptionsPtr )
{
    SessionParametersMap sessionParametersMap;

    while ( 0 != aConnectionInfoOptionsPtr->keyword )
    {
        try
        {
            std::string keyword                    = aConnectionInfoOptionsPtr->keyword  ? aConnectionInfoOptionsPtr->keyword  : std::string();
            std::string environmentVariableVersion = aConnectionInfoOptionsPtr->envvar   ? aConnectionInfoOptionsPtr->envvar   : std::string();
            std::string compiledVersion            = aConnectionInfoOptionsPtr->compiled ? aConnectionInfoOptionsPtr->compiled : std::string();
            std::string currentValue               = aConnectionInfoOptionsPtr->val      ? aConnectionInfoOptionsPtr->val      : std::string();
            std::string dialogLabel                = aConnectionInfoOptionsPtr->label    ? aConnectionInfoOptionsPtr->label    : std::string();
            std::string dialogDisplayCharacter     = aConnectionInfoOptionsPtr->dispchar ? aConnectionInfoOptionsPtr->dispchar : std::string();
            int dialogDisplaysize                  = aConnectionInfoOptionsPtr->dispsize;
            
            SessionParameters connectionParameters( keyword,
                                                    environmentVariableVersion,
                                                    compiledVersion,
                                                    currentValue,
                                                    dialogLabel,
                                                    dialogDisplayCharacter,
                                                    dialogDisplaysize
                                                  );
            
            sessionParametersMap.insert( SessionParametersMap::value_type( connectionParameters.keyword(), connectionParameters ) );
        }
        catch ( std::bad_alloc & )
        {
        }
        
        ++aConnectionInfoOptionsPtr;
    }
    
    return sessionParametersMap;
}
    
SessionParametersMap
SessionHandle::connectionDefaultParameters()
{
    PQconninfoOption* ptrConnInfoOptions = PQconndefaults();
    
    PQConnectionInfoOptionsFree connectionOptionsFreeer( ptrConnInfoOptions );

    return setConnectionInfoParameters( ptrConnInfoOptions );
}

SessionParametersMap
SessionHandle::connectionParameters() const
{
    if ( ! isConnected() )
    {
		throw NotConnectedException();
    }

    PQconninfoOption* ptrConnInfoOptions = 0;
	{
        Poco::FastMutex::ScopedLock mutexLocker( _sessionMutex );
    
        ptrConnInfoOptions = PQconninfo( _pConnection );
    }
    
    PQConnectionInfoOptionsFree connectionOptionsFreeer( ptrConnInfoOptions );
    
    return setConnectionInfoParameters( ptrConnInfoOptions );
}

}}} // Poco::Data::PostgreSQL