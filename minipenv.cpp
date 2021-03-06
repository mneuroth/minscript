/**************************************************************************
 *
 *	project				 : minscript
 *
 *	copyright            : (C) 1999-2019 by Michael Neuroth
 *
 ***************************************************************************/
/***************************************************************************
 *																		   *
 * This file is part of the minscript package,                             *
 * a simple C++/Java like script interpreter.                              *
 *																		   *
 * Copyright (C) 1999-2003 by Michael Neuroth.							   *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 2 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * If you want to use this minscript package in commercial software not    *
 * conform with the GPL please contact the author.                         *
 *                                                                         *
 *  Homepage: https://github.com/mneuroth/minscript                        *
 *                                                                         *
 ***************************************************************************/

#include "platform.h"

#include "mstl_algorithm.h"		// fuer find()
#include "mstl_iostream.h"		// fuer cout

#include "minipenv.h"
#include "mintokenizer.h"		// fuer: _TRUE
#include "minparsernodes.h"		// fuer: minFunctionDeclarationNode

#include <stdio.h>				// fuer: sprintf()
#include <stdlib.h>				// fuer: atof, atol
//#include <locale>
#include <sstream>

const char * g_sClassMethodSeparator = "#";		// war mal "_"
const char * g_sFunctionCallStart =	"?";

extern void DumpScript(const string & sScript, int nLineCodeOfAddedCode, int nCurrentLineNo, list<int> lstBreakpointLines, bool onlyCurrentLine = false);
extern string GetFileNameFromScript( const vector<string> & lines );
extern vector<string> split( const string & str, const string & delimiters );

#undef _old_name_search			// fuer binaere Suche, neu seit 14.2.2003

#undef _DEBUGGING_DEBUGGER

//*************************************************************************

// private Klasse zur Behandlung von Cast-Fehlern
class minCastError
{
public:
	minCastError( const string & sMsg )
		: m_sMsg( sMsg )
	{
	}

	string GetMsg() const
	{
		return m_sMsg;
	}

private:
	string	m_sMsg;
};

//*************************************************************************

bool IsSimpleType( InterpreterValueType aType )
{
	if( aType==Object || aType==Array || aType==Function )
		return false;
	return true;
}

/*
// ACHTUNG: einer der beiden Seiten kann Unknown sein !
minInterpreterType GetMaxType( minInterpreterType aLeftType, minInterpreterType aRightType )
{
// TODO: erweitern und verbessen!!!

	// sind beide Seiten vom gleichen Typ, diesen Typ liefern
	if( aLeftType == aRightType )
		return aLeftType;

	// Pointer und Referenzen behandeln ... --> liefern einen Fehler !
	if( aLeftType.IsPointer() || aRightType.IsPointer() ||
		aLeftType.IsReference() || aRightType.IsReference() )
		return minInterpreterType( Unknown );				

	// User-Datentypen behandeln, ggf. Vererbungs-Beziehungen beachten
	// sobald einer der beiden Seiten Object ist, Object liefern
	if( aLeftType.IsObject() || aRightType.IsObject() )
		return minInterpreterType( Object );

	// sobald einer der beiden Seiten String ist, String liefern
	if( aLeftType.IsString() || aRightType.IsString() )
		return minInterpreterType( String );

	// ist dann einer der Teilnehmer ein double, double liefern
	if( aLeftType.GetType()==Double || aRightType.GetType()==Double )
		return minInterpreterType( Double );

	// ist dann einer der Teilnehmer ein int, int liefern
	if( aLeftType.GetType()==Int || aRightType.GetType()==Int )
		return minInterpreterType( Int );

	// ist dann einer der Teilnehmer ein char, char liefern
	if( aLeftType.GetType()==CharTT || aRightType.GetType()==CharTT )
		return minInterpreterType( CharTT );

	// ist dann einer der Teilnehmer ein bool, bool liefern
	if( aLeftType.GetType()==Bool || aRightType.GetType()==Bool )
		return minInterpreterType( Bool );

	// ist dann einer der Teilnehmer ein void, void liefern
	if( aLeftType.GetType()==Void || aRightType.GetType()==Void )
		return minInterpreterType( Void );

	return minInterpreterType( Unknown );
}
*/

//*************************************************************************

string minInterpreterType::GetRealTypeString() const
{
	return GetTypeStringHelper( /*bManglingName*/false, /*bWithPtrRef*/false );
}

string minInterpreterType::GetTypeString() const
{
	return GetTypeStringHelper( /*bManglingName*/false, /*bWithPtrRef*/true );
}

string minInterpreterType::GetManglingString() const
{
	return GetTypeStringHelper( /*bManglingName*/true, /*bWithPtrRef*/true );
}

string minInterpreterType::GetTypeStringHelper( bool bManglingName, bool bWithPtrRef ) const
{
	string sTypeOut;

	if( IsConst() )
	{
		if( bManglingName )
		{
			sTypeOut = _CONST_MANGLING;
		}
		else
		{
			sTypeOut = "const ";
		}
	}

	switch( m_aType )
	{
		case Unknown :
			sTypeOut += "unknown";
			break;
		case String :
			sTypeOut += _STRING_MS;
			break;
		case Double :
			sTypeOut += _DOUBLE;	// float
			break;
		case Int :
			sTypeOut += _INT;
			break;
		case CharTT :
			sTypeOut += _CHAR;
			break;
		case Bool :
			sTypeOut += _BOOL;
			break;
		case Void :
			sTypeOut += _VOID_MS;
			break;
		case Object :
			sTypeOut += m_sClassName;	//"object";	// zum Testen
			break;
		case Array :
			sTypeOut += "array";			// zum Testen
			break;
		case Function :
			sTypeOut += _FUNCTION;		// zum Testen
			break;
		default:
			return m_sClassName;		// dann ist es ggf. ein User-Typ (Bugfix fuer typedef 26.12.1999)
	};

	if( bWithPtrRef )
	{
		for( int i=0; i<m_nPointerLevel; i++ )
		{
			if( bManglingName )
				sTypeOut += g_sPointerSeparator;
			else
				sTypeOut += g_sPointerSeparator;
		}

		if( bManglingName )
			sTypeOut += (IsReference() ? g_sReferenceSeparator : "" );
		else
			sTypeOut += (IsReference() ? g_sReferenceSeparator : "" );
	}

	return sTypeOut;
}

bool minInterpreterType::GenerateCppCode( string & sCodeOut ) const
{
	sCodeOut += GetTypeString();

	for( int i=0; i<m_nPointerLevel; i++ )
		sCodeOut += " *";

	sCodeOut += (IsReference() ? " & " : " " );

	return true;
}

//*************************************************************************

MEMORY_DBG( static int g_cValueCount = 0; )

MEMORY_DBG( int GetValueCount() { return g_cValueCount; } )


minInterpreterValue::minInterpreterValue()
{
    MEMORY_DBG_OUT( cout << "minInterpreterValue 1 this=0x" << (void *)this << endl; )
    MEMORY_DBG( g_cValueCount++; )
	//m_bIsOwner = true;
	m_aType = Void;
	m_aValue.m_dValue = 0;		// damit diese Variable vernuenftig initialisiert wird !
}

minInterpreterValue::minInterpreterValue( double dVal )
{
    MEMORY_DBG_OUT( cout << "minInterpreterValue 2 this=0x" << (void *)this << endl; )
    MEMORY_DBG( g_cValueCount++; )
	//m_bIsOwner = true;
	m_aType = Double;
	m_aValue.m_dValue = dVal;
}

minInterpreterValue::minInterpreterValue( void * pVal )
{
    MEMORY_DBG_OUT( cout << "minInterpreterValue 4b this=0x" << (void *)this << endl; )
    MEMORY_DBG( g_cValueCount++; )
	//m_bIsOwner = true;
	m_aType = Int;
	m_aValue.m_dValue = 0;		// damit diese Variable vernuenftig initialisiert wird !
	m_aValue.m_lValue = (long long)pVal; 
}

#ifdef _with_bool
minInterpreterValue::minInterpreterValue( bool bVal )
{
    MEMORY_DBG_OUT( cout << "minInterpreterValue 3 this=0x" << (void *)this << endl; )
    MEMORY_DBG( g_cValueCount++; )
	//m_bIsOwner = true;
	m_aType = Bool;
	m_aValue.m_dValue = 0;		// damit diese Variable vernuenftig initialisiert wird !
	m_aValue.m_lValue = (long)bVal;
}
#endif

minInterpreterValue::minInterpreterValue( int iVal )
{
    MEMORY_DBG_OUT( cout << "minInterpreterValue 4 this=0x" << (void *)this << endl; )
    MEMORY_DBG( g_cValueCount++; )
	//m_bIsOwner = true;
	m_aType = Int;
	m_aValue.m_dValue = 0;		// damit diese Variable vernuenftig initialisiert wird !
	m_aValue.m_lValue = (long)iVal; 
}

minInterpreterValue::minInterpreterValue( long lVal )
{
    MEMORY_DBG_OUT( cout << "minInterpreterValue 4 this=0x" << (void *)this << endl; )
    MEMORY_DBG( g_cValueCount++; )
	//m_bIsOwner = true;
	m_aType = Int;
	m_aValue.m_dValue = 0;		// damit diese Variable vernuenftig initialisiert wird !
	m_aValue.m_lValue = lVal; 
}

minInterpreterValue::minInterpreterValue( long long lVal )
{
	MEMORY_DBG_OUT(cout << "minInterpreterValue 4b this=0x" << (void *)this << endl; )
	MEMORY_DBG(g_cValueCount++; )
	//m_bIsOwner = true;
	m_aType = Int;
	m_aValue.m_dValue = 0;		// damit diese Variable vernuenftig initialisiert wird !
	m_aValue.m_lValue = lVal;
}

minInterpreterValue::minInterpreterValue( char cVal )
{ 
    MEMORY_DBG_OUT( cout << "minInterpreterValue 5 this=0x" << (void *)this << endl; )
    MEMORY_DBG( g_cValueCount++; )
	//m_bIsOwner = true;
	m_aType = CharTT; 
	m_aValue.m_dValue = 0;		// damit diese Variable vernuenftig initialisiert wird !
	m_aValue.m_lValue = (long)cVal;
}

minInterpreterValue::minInterpreterValue( const string & sVal )			
{ 
    MEMORY_DBG_OUT( cout << "minInterpreterValue 6 this=0x" << (void *)this << endl; )
    MEMORY_DBG( g_cValueCount++; )
	//m_bIsOwner = true;
	m_aType = String; 
	m_aValue.m_dValue = 0;		// damit diese Variable vernuenftig initialisiert wird !
	m_aValue.m_psValue = new string( sVal ); 
}

minInterpreterValue::minInterpreterValue( const char * sVal )				
{ 
    MEMORY_DBG_OUT( cout << "minInterpreterValue 7 this=0x" << (void *)this << endl; )
    MEMORY_DBG( g_cValueCount++; )
	//m_bIsOwner = true;
	m_aType = String; 
	m_aValue.m_dValue = 0;		// damit diese Variable vernuenftig initialisiert wird !
	m_aValue.m_psValue = new string( sVal ); 
}

// zum Erzeugen einer zweiten Verpackung fuer ein Objekt (CallStackItem) !
minInterpreterValue::minInterpreterValue( minHandle<minCallStackItem>	* phCallStackItem, minInterpreterType aType )
{
    MEMORY_DBG_OUT( cout << "minInterpreterValue 8 this=0x" << (void *)this << endl; )
    MEMORY_DBG( g_cValueCount++; )
	//m_bIsOwner = true;
	m_aType = aType; 
	// Bugfix 4.2.2003:
	// erzeuge einen neuen Handle auf die Daten des uebergebenen Handles
	m_aValue.m_dValue = 0;		// damit diese Variable vernuenftig initialisiert wird !
	m_aValue.m_phObjValue = new minHandle<minCallStackItem>( *phCallStackItem );
}

minInterpreterValue::minInterpreterValue( minInterpreterType aType )
{ 
    MEMORY_DBG_OUT( cout << "minInterpreterValue 9 this=0x" << (void *)this << endl; )
    MEMORY_DBG( g_cValueCount++; )
	//m_bIsOwner = true;
	m_aType = aType; 
	m_aValue.m_dValue = 0;		// damit diese Variable vernuenftig initialisiert wird !

	if( !m_aType.IsReference() && !m_aType.IsPointer() && (m_aType.IsObject() || m_aType.IsArray()) )
	{
		m_aValue.m_phObjValue = new minHandle<minCallStackItem>( new minCallStackItem( (m_aType.IsObject() ? "Object" : "Array") ) );
	}
}

// zum Erzeugen von Zeigern (wird im new-Operator-Node und im Address-Node verwendet)!
minInterpreterValue::minInterpreterValue( minInterpreterType aType, minInterpreterValue * pIpVal/*, bool bIsOwner*/ )	
{
    MEMORY_DBG_OUT( cout << "minInterpreterValue 10 this=0x" << (void *)this << endl; )
    MEMORY_DBG( g_cValueCount++; )
	//m_bIsOwner = bIsOwner;
	m_aType = aType; 
	m_aValue.m_dValue = 0;		// damit diese Variable vernuenftig initialisiert wird !
	m_aValue.m_pPointer = pIpVal;
}

// zum Erzeugen von Funktionszeigern:
minInterpreterValue::minInterpreterValue( minHandle<minFunctionDeclarationNode> hFunction )			
{ 
    MEMORY_DBG_OUT( cout << "minInterpreterValue 11 this=0x" << (void *)this << endl; )
    MEMORY_DBG( g_cValueCount++; )
	//m_bIsOwner = true;
	m_aType = Function; 
	m_aValue.m_dValue = 0;		// damit diese Variable vernuenftig initialisiert wird !
	m_aValue.m_phFunction = new minHandle<minFunctionDeclarationNode>();
	*m_aValue.m_phFunction = hFunction; 
}

// Kopieren des Interpreter-Values
minInterpreterValue::minInterpreterValue( const minInterpreterValue & aObj )						
{ 
    MEMORY_DBG_OUT( cout << "minInterpreterValue 12 (const) this=0x" << (void *)this << " " << (void *)&aObj << endl; )
    MEMORY_DBG( g_cValueCount++; )
	Assign( aObj ); 
}

// Kopieren des Interpreter-Values
minInterpreterValue & minInterpreterValue::operator=( const minInterpreterValue & aObj )			
{
	// ggf. alte Daten loeschen (falls Eigentuemer an Zeigern)
	DeleteDataIfOwner();

	// dann Zuweisung durchfuehren
	Assign( aObj ); 

	return *this; 
}

minInterpreterValue::~minInterpreterValue()
{
    MEMORY_DBG_OUT( cout << "~minInterpreterValue " << (void *)this << endl; )
    MEMORY_DBG( g_cValueCount--; )
	DeleteDataIfOwner();
}

void minInterpreterValue::SetCreator( const minCreatorInterface * pVariableCreator )		
{ 
	if( IsObject() )
	{
		(*(m_aValue.m_phObjValue))->SetCreator( pVariableCreator );
	}
}

string minInterpreterValue::GetString( bool bDebugOutput )	const
{
	if( IsReference() )
	{
		return m_aValue.m_pToReferenceValue->GetString( bDebugOutput );
	}
	else if( m_aType.IsString() )
	{
		return *m_aValue.m_psValue;
	}
	// in allen anderen Faellen konvertieren
    return ConvertTo( String, bDebugOutput ).GetString();
}

double minInterpreterValue::GetDouble() const
{
	if( IsReference() )
	{
		return m_aValue.m_pToReferenceValue->GetDouble();
	}
	if( m_aType.GetType()==Double )
	{
		return m_aValue.m_dValue;
	}
	return ConvertTo( Double ).GetDouble();
}

bool minInterpreterValue::GetBool() const
{
	if( IsReference() )
	{
		return m_aValue.m_pToReferenceValue->GetBool();
	}
	if( m_aType.GetType()==Bool )
	{
		return m_aValue.m_lValue!=0;
	}
	return ConvertTo( Bool ).GetBool();
}

long long minInterpreterValue::GetInt() const
{
	if( IsReference() )
	{
		return m_aValue.m_pToReferenceValue->GetInt();
	}
	if( m_aType.GetType()==Int )
	{
		return m_aValue.m_lValue;
	}
	return ConvertTo( Int ).GetInt();
}

char minInterpreterValue::GetChar() const
{
	if( IsReference() )
	{
		return m_aValue.m_pToReferenceValue->GetChar();
	}
	if( m_aType.GetType()==CharTT )
	{
		return (char)m_aValue.m_lValue;
	}
	return ConvertTo( CharTT ).GetChar();
}

int minInterpreterValue::GetSizeofValue() const
{
	if( IsPointer() )
	{
		return sizeof( m_aValue.m_pPointer );
	}
	if( IsReference() )
	{
		return m_aValue.m_pToReferenceValue->GetSizeofValue();
	}
	switch( m_aType.GetType() )
	{
		case Bool:
		case CharTT:
		case Int:
			return sizeof( m_aValue.m_lValue );
		case Double:
			return sizeof( m_aValue.m_dValue );
		case String:
			return sizeof( *(m_aValue.m_psValue) );
		case Object:
		case Array:
			return (*(m_aValue.m_phObjValue))->GetSizeofValue();
		case Function:
			return sizeof( *(m_aValue.m_phFunction) );

		default:
			return sizeof( minIpValueImplHelper );
	}
	return 0;
}

void minInterpreterValue::SetRef( minInterpreterValue * pIpVal, minInterpreterType aType )	
{ 
	// ggf. alte Daten loeschen (falls Eigentuemer an Zeigern)
	DeleteDataIfOwner();

	m_aType = aType; 
	m_aType.m_bIsReference = true;				// TODO: unschoener Konstruktor... Was ist mit Referenz auf Referenz ?
	m_aValue.m_pToReferenceValue = pIpVal; 
}

void minInterpreterValue::SetType( minInterpreterType aNewType )
{
	// muss ueberhaupt etwas getan werdne ?
	if( m_aType != aNewType )
	{
		// ggf. alte Daten loeschen (falls Eigentuemer an Zeigern)
		DeleteDataIfOwner();

		m_aType = aNewType;

		// Variable vernuenftig initialisieren !
		Assign( minInterpreterValue( aNewType ) );
	}
}

minInterpreterValue minInterpreterValue::CopyValue() const
{
	minInterpreterValue aRetVal;

	if( IsReference() )
	{
		aRetVal.Assign( *(m_aValue.m_pToReferenceValue) );
	}
	else
	{
		aRetVal.Assign( *this );
	}

	return aRetVal;
}

minInterpreterValue * minInterpreterValue::GetAddress()
{
	if( IsReference() )
		return m_aValue.m_pToReferenceValue;
	return this;
}

minInterpreterValue * minInterpreterValue::GetDereference()
{
	if( IsReference() )
	{
		// Anfrage an das eigentliche Objekt weiterleiten,
		// dieses Objekt ist nur eine Referenz !
		//if( m_aValue.m_pToReferenceValue )	// muss immer gesetzt sein
			return (*(m_aValue.m_pToReferenceValue)).GetDereference();		// Bugfix 31.1.2003
	}
	else if( IsPointer() )
	{
		return m_aValue.m_pPointer;
	}
	return 0;	// !ok
}

bool minInterpreterValue::DeletePointer()
{
	if( IsReference() )
	{
		// Anfrage an das eigentliche Objekt weiterleiten,
		// dieses Objekt ist nur eine Referenz !
		//( m_aValue.m_pToReferenceValue )
			return m_aValue.m_pToReferenceValue->DeletePointer();
	}
	else if( IsPointer() )
	{
		// die Eigentuemerschaft an dem Zeiger wird durch dieses Delete aufgehoben ! (Bugfix 30.12.1999, linux !)

		if( m_aValue.m_pPointer )
		{
			delete m_aValue.m_pPointer;
			m_aValue.m_pPointer = 0;
		}
		else
		{
			cerr << "Error: deleting 0 pointer !" << endl;
		}

		return true;
	}
	return false;
}

bool minInterpreterValue::CastAndAssign( const minInterpreterValue & aObj, string & sErrorMsg )
{
	try {
		if( IsReference() )
		{
			if( m_aValue.m_pToReferenceValue )
				return m_aValue.m_pToReferenceValue->CastAndAssign( aObj, sErrorMsg );
			else
				return false;
		}
		else if( IsPointer() )
		{
			if( (m_aType.GetPointerLevel()==aObj.m_aType.GetPointerLevel()) || aObj.IsNull() )	// 0 darf einem Zeiger zugewiesen werden !
			{
				// ggf. alte Daten loeschen (falls Eigentuemer an Zeigern)
				DeleteDataIfOwner();
				// und dann Zuweisung durchfuehren
				Assign( aObj.ConvertTo( m_aType ) );
			}
			else
				return false;
		}
		else
		{
			// Bugfix 7.1.2003: Zuweisungen an Objekt nur durch anderes Objekt moeglich
			//                  d.h. kein aObj = 7; erlauben !
			if( IsObject() && !aObj.IsObject() )
			{
				return false;
			}
			if( IsArray() && !aObj.IsArray() )
			{
				return false;
			}

			// ggf. alte Daten loeschen (falls Eigentuemer an Zeigern)
			DeleteDataIfOwner();
			// und dann Zuweisung durchfuehren
			Assign( aObj.ConvertTo( m_aType ) );
		}
	}
	catch( minCastError & aExc )
	{
		// wenigstens einen Dummy zuweisen, damit Daten konsistent bleiben,
		// bzw. keine haengende Referenz existiert !
		Assign( minInterpreterValue() );
		sErrorMsg = aExc.GetMsg();
		return false;
	}

	return true;
}

minHandle<minCallStackItem> * minInterpreterValue::GetObjectCallStackItem()
{
	if( IsReference() )
	{
		// ggf. Anfrage an das eigentliche Objekt weiterleiten,
		// dieses Objekt ist nur eine Referenz !
		if( m_aValue.m_pToReferenceValue )
			return m_aValue.m_pToReferenceValue->GetObjectCallStackItem();
		else
			return 0; //minHandle<minCallStackItem>();		// Error, sollte NIE vorkommen
	}
/*
	else if( IsPointer() )
	{
		//if( (*(m_aValue.m_phPointer)).GetPtr() == this )
		//	return *(m_aValue.m_phPointer);
		return (*(m_aValue.m_phPointer))->GetObjectCallStackItem();
	}
*/
	else
	{
		return m_aValue.m_phObjValue;
	}
}

minHandle<minFunctionDeclarationNode> minInterpreterValue::GetFunction()
{ 
	if( IsReference() )
	{
		// ggf. Anfrage an das eigentliche Objekt weiterleiten,
		// dieses Objekt ist nur eine Referenz !
		if( m_aValue.m_pToReferenceValue )
			return m_aValue.m_pToReferenceValue->GetFunction();
		else
			return 0;
	}
	else
	{
		return *m_aValue.m_phFunction; 
	}
}

bool minInterpreterValue::IsNull() const
{ 
	return (m_aType.GetType()==Int && m_aValue.m_lValue==0) ||
		   (m_aType.GetType()==Double && m_aValue.m_dValue==0); 
}

string minInterpreterValue::GetInfoString() const							
{ 
	return GetString(); 
}

void * minInterpreterValue::GetRepAddr() const
{
	switch( m_aType.GetType() )
	{
		case String :
			return (void *)m_aValue.m_psValue;
		case Double :
			return (void *)&(m_aValue.m_dValue);
		case Bool :
		case CharTT :
		case Int :
			return (void *)&(m_aValue.m_lValue);
		case Object:
			return (void *)(*(m_aValue.m_phObjValue)).GetPtr();
		
		default:
			return 0;
	}
}

minInterpreterValue minInterpreterValue::ConvertTo( const minInterpreterType & aToType, bool bDebugOutput ) const
{
	if( IsReference() )
	{
		// Anfrage an das eigentliche Objekt weiterleiten,
		// dieses Objekt ist nur eine Referenz !
		if( m_aValue.m_pToReferenceValue )
		{
			return m_aValue.m_pToReferenceValue->ConvertTo( aToType );
		}
	}
	else if( IsPointer() )
	{
		// ist ueberhaupt eine Umwandlung notwendig ?
		if( aToType == m_aType )
			return *this;

		if( m_aValue.m_pPointer == 0 )
		{
			return minInterpreterValue( 0 );
		}

// TODO: Behandlung wenn aToType z.B. ein int * * und this ein int * ist !!!
		switch( aToType.GetType() )
		{
			case String : 
				{
					char sBuffer[c_iMaxBuffer];

					sprintf( sBuffer, "0x%llx", (unsigned long long)(*(m_aValue.m_pPointer)).GetRepAddr() );
					return minInterpreterValue( sBuffer );
				}
				break;
			case Double : 
				return minInterpreterValue( (long long)(*(m_aValue.m_pPointer)).GetRepAddr() );
			case Bool :
				if( m_aValue.m_pPointer==0 )
					return minInterpreterValue( 0 );
				else
					return minInterpreterValue( 1 );
			case CharTT :
			case Int : 
				return minInterpreterValue( (long long)(*(m_aValue.m_pPointer)).GetRepAddr() );
			case Object :
				// nur Zuweisungen von Obj-Pointer nach Obj-Pointer sind zulaessig !
				if( aToType.IsPointer() )
				{
					return minInterpreterValue( aToType, m_aValue.m_pPointer );
				}
			default:
				throw minCastError( "can not convert into pointer" );
// min todo gulp --> was ist mit Array Behandlung ?
		}
	}
	else
	{
		// ist ueberhaupt eine Umwandlung notwendig ?
		if( m_aType == aToType )
			return *this;

		// Behandlung aller Pointer-Konversionen !!! 
		// Bugfix 3.2.2003
		// Null-Zuweisungen an Pointer immer zulassen !
		if( aToType.IsPointer() && (m_aValue.m_lValue==0) )
		{
			return minInterpreterValue( aToType, 0 );
		}

		if( m_aType.GetType() == String )		// Hier kann noch string * stehen !!!
		{
			switch( aToType.GetType() )
			{
				case Double : 
					return minInterpreterValue( atof( m_aValue.m_psValue->c_str() ) );
				case Bool :
					if( *m_aValue.m_psValue==string( _TRUE ) )
						return minInterpreterValue( (bool)1 );
					else
						return minInterpreterValue( (bool)0 );
				case CharTT :
					// interpretiere den String als eine Zahl und weise diese zu !
					return minInterpreterValue( (char)atol( m_aValue.m_psValue->c_str() ) );
				case Int : 
					// interpretiere den String als eine Zahl und weise diese zu !
					return minInterpreterValue( (int)atol( m_aValue.m_psValue->c_str() ) );
				default:
					throw minCastError( "can not convert into string" );
			}
		}
		if( m_aType.GetType() == Double )
		{
			switch( aToType.GetType() )
			{
				case String :
					{
						char sBuffer[c_iMaxBuffer];

						sprintf( sBuffer, "%lf", m_aValue.m_dValue );
						return minInterpreterValue( sBuffer );
					}
				case Bool :
					return minInterpreterValue( ((int)m_aValue.m_dValue)!=0 );
				case CharTT :
					return minInterpreterValue( (char)m_aValue.m_dValue );
				case Int : 
					return minInterpreterValue( (int)m_aValue.m_dValue );
				default:
					throw minCastError( "can not convert into double" );
			}
		}
		if( m_aType.GetType() == Int )
		{
			switch( aToType.GetType() )
			{
				case String :
					{
						char sBuffer[c_iMaxBuffer];

						sprintf( sBuffer, "%lld", m_aValue.m_lValue );
						return minInterpreterValue( sBuffer );
					}
					break;
				case Double : 
					return minInterpreterValue( (double)m_aValue.m_lValue );
				case CharTT :
					return minInterpreterValue( (char)m_aValue.m_lValue );
				case Bool :
					return minInterpreterValue( m_aValue.m_lValue!=0 );
				default:
					throw minCastError( "can not convert into int" );
			}
		}
		if( m_aType.GetType() == CharTT )
		{
			switch( aToType.GetType() )
			{
				case String :
					{
						string sTemp;
						sTemp = (char)m_aValue.m_lValue;
						return minInterpreterValue( sTemp ); 
					}
				case Double : 
					return minInterpreterValue( (double)m_aValue.m_lValue );
					break;
				case Bool :
					return minInterpreterValue( m_aValue.m_lValue!=0 );
				case Int : 
					return minInterpreterValue( (int)m_aValue.m_lValue );
				default:
					throw minCastError( "can not convert into char" );
			}
		}
		if( m_aType.GetType() == Bool )
		{
			switch( aToType.GetType() )
			{
				case String :
					return minInterpreterValue( (m_aValue.m_lValue ? _TRUE : _FALSE) );
				case Double :
					return minInterpreterValue( m_aValue.m_dValue );
				case CharTT :
					return minInterpreterValue( (char)m_aValue.m_lValue );
				case Int : 
					return minInterpreterValue( (int)m_aValue.m_lValue );
				default:
					throw minCastError( "can not convert into bool" );
			}
		}
		if( m_aType.GetType() == Void )
		{
			switch( aToType.GetType() )
			{
				case String :
					return minInterpreterValue( "void" );
				case Double : 
				case CharTT :
				case Int : 
					return minInterpreterValue( 0 );
				default:
					throw minCastError( "can not convert into void" );
			}
		}
		if( m_aType.GetType() == Object )
		{
			switch( aToType.GetType() )
			{
				case String :
					{
                        string sVariables;
                        if( bDebugOutput && m_aValue.m_phObjValue!=0 )
                        {
                            sVariables = (*(m_aValue.m_phObjValue))->GetInfoString(true);
                        }
						char sBuffer[c_iMaxBuffer];
                        sprintf( sBuffer, "object 0x%llx %s", (unsigned long long)(m_aValue.m_phObjValue ? (*(m_aValue.m_phObjValue)).GetPtr() : 0), sVariables.c_str() );
						return minInterpreterValue( /*"object"*/sBuffer );
					}
				case Double : 
					return minInterpreterValue( (double)-1.0 );
				case CharTT :
					return minInterpreterValue( (char)-1 );
				case Int : 
					return minInterpreterValue( (int)-1 );
				case Object:
					throw minCastError( "can not convert objects into other objects" );
					//return minInterpreterValue( minInterpreterType( aToType ) );
				default:
					throw minCastError( aToType.GetTypeString() + " can not be convert into an object" );
			}
		}
		if( m_aType.GetType() == Array )
		{
			switch( aToType.GetType() )
			{
				case String :
					{
                        string sVariables;
                        if( bDebugOutput && m_aValue.m_phObjValue!=0 )
                        {
                            sVariables = (*(m_aValue.m_phObjValue))->GetInfoString(true);
                        }
                        char sBuffer[c_iMaxBuffer];
                        sprintf( sBuffer, "Array 0x%llx %s", (unsigned long long)(m_aValue.m_phObjValue ? (*(m_aValue.m_phObjValue)).GetPtr() : 0), sVariables.c_str() );
						return minInterpreterValue( /*"object"*/sBuffer );
					}
				case Double : 
					return minInterpreterValue( (double)-1.0 );
				case CharTT :
					return minInterpreterValue( (char)-1 );
				case Int : 
					return minInterpreterValue( (int)-1 );
				case Object:
					throw minCastError( "can not convert arrrays into objects" );
					//return minInterpreterValue( minInterpreterType( aToType ) );
				default:
					throw minCastError( "can not convert into array" );
			}
		}
		if( m_aType.GetType() == Function )
		{
			switch( aToType.GetType() )
			{
				case String :
					return minInterpreterValue( _FUNCTION );
				case Double : 
					return minInterpreterValue( (double)-1.0 );
				case CharTT :
					return minInterpreterValue( (char)-1 );
				case Int : 
					return minInterpreterValue( (int)-1 );
				default:
					throw minCastError( "can not convert into function" );
			}
		}
	}

	return minInterpreterValue();	// Fehler liefern
}	

void minInterpreterValue::Assign( const minInterpreterValue & aObjX )
{
	// Bugfix 28.1.2003:
	// jetzt wird der Wert richtig kopiert, auch wenn es
	// eine Referenz ist
	// Zum Erzeugen von Referenzen verwende die Methode
	// SetRef().
	/*
	const minInterpreterValue * pRealCopyObj = &aObjX;
	// dieses Deep-Copy interferriert mit EvaluateArguments() in minparsernodes.cpp !!!
	// man kann dann keine Referenz Objekte erzeugen !!! d.h. Probleme beim call by reference !
	// verwende CopyValue() um einen echten Wert ohne Refernz zu bekommen !
	if( aObjX.IsReference() )
	{
		pRealCopyObj = aObjX.m_aValue.m_pToReferenceValue;
	}
	const minInterpreterValue & aObj = *pRealCopyObj;
	*/
	const minInterpreterValue & aObj = aObjX;

	// Bugfix 28.1.2003: Unterscheidung ob Referenz
	if( m_aType.IsReference() )
	{
		*(m_aValue.m_pToReferenceValue) = aObj;

		// dies sollte nie auftreten, falls doch durch einen assert 'absichern'
		assert( m_aType.GetType() == aObj.GetInterpreterType().GetType() );
		/*
		if( m_aType.GetType() != aObj.GetInterpreterType().GetType() )
		{
			cerr << "Error: Bad assign found !!!" << endl;
			exit( 99 );
		}
		*/
	}
	else
	{
		m_aType = aObj.m_aType;
		m_aValue = aObj.m_aValue;
		//m_bIsOwner = aObj.m_bIsOwner;
	}
	// nun noch die Faelle die mit deep-copy behandelt werden muessen
	// - Referenzen muessen nicht spezial behandelt werden
	if( !m_aType.IsReference() )
	{
		// - Strings
		if( m_aType.IsString() )
		{
			m_aValue.m_psValue = new string( (aObj.m_aValue.m_psValue ? *aObj.m_aValue.m_psValue : "") );
		}
		// - Objekte 
		if( m_aType.IsObject() || m_aType.IsArray() )
		{
			m_aValue.m_phObjValue = new minHandle<minCallStackItem>( (aObj.m_aValue.m_phObjValue ? *aObj.m_aValue.m_phObjValue : 0 ) );
		}
		// - Zeiger
		if( m_aType.IsPointer() )
		{
			// Bugfix 3.2.2003: Zeiger nur explizit (mit DeletPointer()) loeschen !!!			
			//                  d.h. werden auch nicht mehr (deep) kopiert.
			//					die Zuweisung des m_aValue oben reicht aus.
			/*
			if( aObj.m_aValue.m_pPointer )
				m_aValue.m_pPointer = new minInterpreterValue( *(aObj.m_aValue.m_pPointer) );
			else
				m_aValue.m_pPointer = new minInterpreterValue( 0 );
			*/
		}
		// - Funktionszeiger
		if( m_aType.IsFunction() )
		{
			m_aValue.m_phFunction = new minHandle<minFunctionDeclarationNode>( (aObj.m_aValue.m_phFunction ? *aObj.m_aValue.m_phFunction : 0) );
		}
	}
}

void minInterpreterValue::DeleteDataIfOwner()
{
	if( !m_aType.IsReference() )
	{
		// - Strings
		if( m_aType.IsString() )
		{
			delete m_aValue.m_psValue;
		}
		// - Objekte
		if( m_aType.IsObject() || m_aType.IsArray() )
		{
			delete m_aValue.m_phObjValue;
		}
		// - Zeiger
		if( m_aType.IsPointer() /*&& m_bIsOwner*/ )
		{
			// Bugfix 3.2.2003: Zeiger nur explizit (mit DeletPointer()) loeschen !!!			
			/*
			if( m_aValue.m_pPointer )
			{
				delete m_aValue.m_pPointer;
			}
			*/
		}
		// - Funktionszeiger
		if( m_aType.IsFunction() )
		{
			delete m_aValue.m_phFunction;
		}
	}
}

//*************************************************************************
//*************************************************************************
//*************************************************************************

MEMORY_DBG( static int g_cItemCount = 0; )

MEMORY_DBG( int GetItemCount() { return g_cItemCount; } )


minCallStackItem::minCallStackItem( const string & sItemName, bool bHidesObject, int nLineNumber ) 
	: m_bHidesObject( bHidesObject ),
	  m_sItemName( sItemName ), 
	  m_sUserName( "" ), 
	  m_nLineNumber( nLineNumber ),
	  m_nCurrentLineNumber( 0 ),
	  m_paBaseObjectList( 0 ),
	  m_pThisObj( 0 ),
	  m_pVariableCreator( 0 )
{
    MEMORY_DBG( g_cItemCount++; )
	//cout << "minCallStackItem() >" << m_sItemName.c_str() << "< this=" << (void *)this << endl;
}

minCallStackItem::minCallStackItem( const minCallStackItem & aOther )
{
    MEMORY_DBG( g_cItemCount++; )
	Assign( aOther );
	//cout << "minCallStackItem(const) >" << m_sItemName.c_str() << "< this=" << (void *)this << endl;
}

minCallStackItem::~minCallStackItem()
{
    MEMORY_DBG( g_cItemCount--; )
	//cout << "~minCallStackItem() >" << m_sItemName.c_str() << "< this=" << (void *)this << endl;
	//cout << "----------> DELETE CallStackItem " << m_sUserName.c_str() << endl;
	if( m_paBaseObjectList )
	{
		delete m_paBaseObjectList;
	}
	if( m_pThisObj )
	{
		delete m_pThisObj;
	}
}

minCallStackItem & minCallStackItem::operator=( const minCallStackItem & aOther )
{
	Assign( aOther );
	//cout << "minCallStackItem = () >" << m_sItemName.c_str() << "< this=" << (void *)this << endl;
	return *this;
}

void minCallStackItem::Assign( const minCallStackItem & aOther )
{
	m_bHidesObject = aOther.m_bHidesObject;
	m_sItemName = aOther.m_sItemName;
	m_sUserName = aOther.m_sUserName;
	m_nLineNumber = aOther.m_nLineNumber;
	m_nCurrentLineNumber = aOther.m_nCurrentLineNumber;
	m_aVariableContainer = aOther.m_aVariableContainer;
	if( aOther.m_paBaseObjectList )
		m_paBaseObjectList = new minBaseObjectList( *(aOther.m_paBaseObjectList) );
	else
		m_paBaseObjectList = 0;
	m_pThisObj = 0;			// wird automatisch in GetThis() on demand angelegt !
	m_pVariableCreator = aOther.m_pVariableCreator;
}

void minCallStackItem::AddBaseObject( minHandle<minCallStackItem> hBaseObject )
{
	if( !m_paBaseObjectList )
	{
		m_paBaseObjectList = new minBaseObjectList();
	}
	m_paBaseObjectList->push_back( hBaseObject );
}

bool minCallStackItem::ExistsVariable( const string & sName ) const
{
	VariableContainerT::const_iterator aIter
			= find( m_aVariableContainer.begin(), m_aVariableContainer.end(), minInterpreterVariable( sName ) );
	// ggf. in Basis-Objekten suchen...
	if( aIter==m_aVariableContainer.end() && m_paBaseObjectList )
	{
		minBaseObjectList::const_iterator aBaseIter = m_paBaseObjectList->begin();

		while( aBaseIter != m_paBaseObjectList->end() )
		{
			if( (*aBaseIter)->ExistsVariable( sName ) )
				return true;
			++aBaseIter;
		}
	}
	return aIter != m_aVariableContainer.end();
}

minHandle<minInterpreterValue> minCallStackItem::GetValueForVariable( const string & sName )
{
    //cout << "GetVariable >" << GetUserName().c_str() << "< this=" << (void *)this << " " << m_sItemName.c_str() << " name=" << sName << " size=" << m_aVariableContainer.size() << endl;

	VariableContainerT::iterator aIter
			= find( m_aVariableContainer.begin(), m_aVariableContainer.end(), minInterpreterVariable( sName ) );
	if( aIter != m_aVariableContainer.end() )
	{
		return (*aIter).GetValue();
	}
	// ggf. in Basis-Objekten suchen...
	if( aIter==m_aVariableContainer.end() && m_paBaseObjectList )
	{
		minBaseObjectList::iterator aBaseIter = m_paBaseObjectList->begin();

		while( aBaseIter != m_paBaseObjectList->end() )
		{
			minHandle<minInterpreterValue> hValue = (*aBaseIter)->GetValueForVariable( sName );
			if( hValue.IsOk() )
				return hValue;
			++aBaseIter;
		}
	}
	return minHandle<minInterpreterValue>();	// nicht gefunden !
}

bool minCallStackItem::AddNewVariable( const string & sName, minHandle<minInterpreterValue> hValue, const string & sAliasName )
{
	// mehrfach angelegte Variablen abweisen:
	if( !ExistsVariable( sName ) )
	{
		m_aVariableContainer.push_back( minInterpreterVariable( sName, hValue, sAliasName ) );
		//cout << "AddNewVariable >" << GetUserName().c_str() << "< this=" << (void *)this << " " << m_sItemName.c_str() << " name=" << sName << " new_size=" << m_aVariableContainer.size() << endl;
		return true;
	}
	return false;
}

void minCallStackItem::DumpVariables(ostream & stream)
{
	stream << "Dump variables:" << endl;

	VariableContainerT::iterator iter = m_aVariableContainer.begin();
	while (iter != m_aVariableContainer.end())
	{
		stream << (*iter).GetName() << "\t" << (*iter).GetValue()->GetString() << "\t" << (*iter).GetValue()->GetTypeString() << endl;
		++iter;
	}
}

string minCallStackItem::GetInfoString(bool bShowContent) const
{
	string sResult = GetItemName();

// TODO gulp working --> wie sollen die Member Daten ausgegeben werden? Diese Methode beeinflusst callstack und local anzeige !!!
//	char sBuffer[c_iMaxBuffer];
/*
	if( GetUserName().length()>0 )
	{
		sprintf( sBuffer, "0x%lx", (unsigned long int)this );
		sResult += "{"+GetUserName()+" "+sBuffer+"}";
	}
	if( m_nLineNumber>0 )
	{
		sprintf(sBuffer, "%d", m_nLineNumber);
		sResult += " call_line=";
		sResult += sBuffer;
	}
	if( m_nCurrentLineNumber>0 )
	{
		sprintf(sBuffer, "%d", m_nCurrentLineNumber);
		sResult += " lineno=";
		sResult += sBuffer;
	}
	sResult += ": ";
*/
	if (bShowContent)
	{
		sResult += " ";
		sResult += "{ ";

		VariableContainerT::const_iterator aIter = m_aVariableContainer.begin();
		while (aIter != m_aVariableContainer.end())
		{
			const minInterpreterVariable & aVar = *aIter;
			sResult += aVar.GetInfoString();
			++aIter;
			if (aIter != m_aVariableContainer.end())
			{
				sResult += "; ";
			}
		}

		sResult += " }";
	}

	return sResult;
}

int minCallStackItem::GetSizeofValue() const
{
	int iTotalSize = sizeof( void * );		// this mit berechnen

	VariableContainerT::const_iterator aIter = m_aVariableContainer.begin();
	while( aIter != m_aVariableContainer.end() )
	{
		const minInterpreterVariable & aVar = *aIter;
		iTotalSize += aVar.GetSizeofValue();
		++aIter;
	}

	return iTotalSize;
}

minHandle<minInterpreterValue> minCallStackItem::GetThis()
{
	/* Bugfix: 4.2.2003

	   Diese Methode legt on demand eine Variable mit dem
	   Namen "this" an.
	   Dazu wird erst eine 'Refernz' auf das eigene Objekt
	   angelegt und anschliessend ein Zeiger auf diese
	   Referenz (neue Verpackung) erzeugen und als Variable
	   an sich selbst ablegen.
	*/

	minHandle<minInterpreterValue> hThis = GetValueForVariable( "this" );

	// gibt es das "this" schon ?
	if( !hThis.IsOk() )
	{
		// NEIN !

		// Eine 'Referenz' auf sich selbst erzeugen, allerdings ohne Eigentuemer
		// an dem Zeiger zu werden
		minInterpreterType aThisType( Object, /*nPointerLevel*/0, /*bIsRef*/false );
		minHandle<minCallStackItem> hTempThis( /*bOwner*/false, this );
		m_pThisObj = new minInterpreterValue( &hTempThis, aThisType );

		// Pointer auf diese Referenz erzeugen
		minInterpreterType aThisPtrType( Object, /*nPointerLevel*/1, /*bIsRef*/false );
		minHandle<minInterpreterValue> hThisPtr( new minInterpreterValue ( aThisPtrType, m_pThisObj/*, false*/ ) );

		// Variable nun anlegen
		AddNewVariable( "this", hThisPtr );

		// jetzt muss es das this aber geben !
		return GetValueForVariable( "this" );
	}
	else
	{
		// JA !

		return hThis;
	}
}

//*************************************************************************

// parse z.B. %int**&%double
static bool GetFirstType( string & sNameInOut, string & sTypeOut )
{
	if( sNameInOut.length()>0 )
	{
		size_t nSeparatorLen = strlen( g_sManglingSeparator );
		if( sNameInOut.substr( 0, nSeparatorLen )==g_sManglingSeparator )
		{
			sNameInOut = sNameInOut.substr( nSeparatorLen, sNameInOut.length()-nSeparatorLen );
			size_t nFound = sNameInOut.find( g_sManglingSeparator );

			if( nFound==string::npos )
			{
				// dann ist es ggf. der letzte String in dem Mangling-String
				nFound = sNameInOut.length();
			}
			sTypeOut = sNameInOut.substr( 0, nFound );
			sNameInOut = sNameInOut.substr( nFound, sNameInOut.length()-nFound );
			
			// neu seit 29.1.2003: ueberpruefe auf const und
			// falls vorhanden entfernen, da dies z.Z. nicht interessiert !
			nFound = sTypeOut.find( _CONST_MANGLING );
			if( nFound!=string::npos )
			{
				sTypeOut = sTypeOut.substr( strlen( _CONST_MANGLING ) );
			}

			return true;
		}
	}

	return false;
}

// Typen muessen bis auf Referenz uebereinstimmen
// TODO --> ggf double <--> int etc. erlauben !
// ACHTUNG: ggf. Vererbung beachten...
static bool IsCompatible( const string & sName1, const string & sName2 )
{
	if( sName1 == sName2 )
		return true;

	// Referenz muss immer am Ende des Strings stehen, wird durch die Mangling-Methoden gewaehrleistet
	// d.h. int***&  oder double&
	size_t nSeparatorLen = strlen( g_sReferenceSeparator );
	string sName1Temp = sName1;
	string sName2Temp = sName2;	
	if( sName1Temp.length()>=nSeparatorLen && sName1Temp.substr( sName1Temp.length()-1, nSeparatorLen )==g_sReferenceSeparator )
		sName1Temp = sName1Temp.substr( 0, sName1Temp.length()-nSeparatorLen );
	if( sName2Temp.length()>=nSeparatorLen && sName2Temp.substr( sName2Temp.length()-1, nSeparatorLen )==g_sReferenceSeparator )
		sName2Temp = sName2Temp.substr( 0, sName2Temp.length()-nSeparatorLen );

	return sName1Temp == sName2Temp;
}

static bool IsManglingNameCompatible( const string & sName1, const string & sName2 )
{
	if( sName1 == sName2 )
		return true;

	string sName1Temp = sName1;
	string sName2Temp = sName2;
	// Vergleiche Argument weise, ignoriere Referenzen
	string sType1, sType2;
	while( GetFirstType( sName1Temp, sType1 ) && GetFirstType( sName2Temp, sType2 ) )
	{
		if( !IsCompatible( sType1, sType2 ) )
			return false;
	}

	// hier duerfen keine Argumente mehr uebrig sein !
	if( sName1Temp.length()>0 || sName2Temp.length()>0 )
		return false;

	// wenn man bis hierher gekommen ist, sind die Mangling-Namen kompatibel
	return true;
}

//*************************************************************************

minInterpreterEnvironment::minInterpreterEnvironment()
{
	m_nLineCountOfAddedCode = 0;
    m_nCurrentLineNo = 0;
	m_nCurrentDebuggerCallStackLevel = 0;
    m_nCurrentCallStackLevel = 0;
	m_nLastBreakpointLineNo = 0;
    m_nLastErrorCode = 0;
	m_bDebug = false;
    m_bDbg = false;
	m_bDbgResetExecution = false;
	m_bIsSilent = false;
	ResetDebuggerExecutionFlags();
}

minInterpreterEnvironment::~minInterpreterEnvironment()
{
	// alle CallStack-Entraege loeschen
	m_aCallStack.erase( m_aCallStack.begin(), m_aCallStack.end() );
	RemoveAllFunctions();
	RemoveAllClasses();
}

void minInterpreterEnvironment::ResetDebuggerExecutionFlags()
{
	m_bRunDbg = false;
	m_bStepToNextLine = false;
	m_bStepIntoNextLine = false;
	m_bStepOut = false;
}

minHandle<minCallStackItem> minInterpreterEnvironment::GetActCallStackItem()
{
	if( m_aCallStack.size()>0 )
	{
		return *m_aCallStack.rbegin();
	}
	return minHandle<minCallStackItem>( 0 );
}

minHandle<minCallStackItem> minInterpreterEnvironment::GetActCallStackItemMinusOne()
{
	if( m_aCallStack.size()>1 )
	{
		return *(++m_aCallStack.rbegin());
	}
	return minHandle<minCallStackItem>( 0 );
}

minHandle<minCallStackItem> minInterpreterEnvironment::GetCallStackItemForLevel( int iLevel )
{
	int i = 1;
	size_t iMax = m_aCallStack.size();
	if( iLevel>=1 && iLevel<=(int)m_aCallStack.size() )
	{
		CallStackContainerT::iterator iter = m_aCallStack.begin();
		while( iter!=m_aCallStack.end() )
		{
			if( i == iLevel )
			{
				return *iter;
			}
			++i;
			++iter;
		}
	}
	return minHandle<minCallStackItem>(0);
}

bool minInterpreterEnvironment::PushCallStackItem( minHandle<minCallStackItem> aCallStackItem )
{
	m_aCallStack.push_back( aCallStackItem );
	return true;
}

bool minInterpreterEnvironment::PushCallStackItem( const string & sItemName, bool bHidesObject, int nLineNumber )
{
	m_aCallStack.push_back( minHandle<minCallStackItem>( new minCallStackItem( sItemName, bHidesObject, nLineNumber ) ) );
	return true;
}

bool minInterpreterEnvironment::PopCallStackItem()
{
 	// Bugfix 23.1.2000: es koennen Objekte mehrfach auf dem Stack stehen, nur das letzte entfernen!
	if( m_aCallStack.size()>0 )
	{
		m_aCallStack.erase( --(m_aCallStack.end()) );
		return true;
	}
	return false;
}

void minInterpreterEnvironment::ProcessError( minInterpreterNode * pCurrentNode )
{
// TODO
	if( !IsSilentMode() )
	{
		cout << "EXCEPTION occured !!!" << endl;
		cout << "  ErrorNo  = " << GetLastErrorCode() << endl;
		cout << "  ErrorMsg = " << GetLastErrorMsg().c_str() << endl;
        cout << "  Line No  = " << pCurrentNode->GetLineNumber() << endl;
        // vorlaeufig nur Stack anzeigen
		Dump();

        if( IsDbgMode() )
        {
            ProcessDbg( pCurrentNode );
        }
	}
	// und jetzt eine Exception aufwerfen
	throw minRuntimeException( GetLastErrorMsg() );
}

string minInterpreterEnvironment::GetInfoString() const
{
	char	sBuffer[c_iMaxBuffer];
	string	sResult;
	int		nCount = 0;
	int		nMaxCount = (int)m_aCallStack.size();
	CallStackContainerT::const_reverse_iterator aIter = m_aCallStack.rbegin();
	while( aIter != m_aCallStack.rend() )
	{
		minHandle<minCallStackItem> hStackItem = *aIter;
		++nCount;
		sprintf( sBuffer, "%d 0x%llx", nMaxCount-nCount+1, (unsigned long long)hStackItem.GetPtr() );
		sResult += "\t" + string( sBuffer ) + " " + hStackItem->GetInfoString() + "\n";
		++aIter;
	}
	return sResult;
}

minHandle<minDebuggerInfo> minDebuggerInfo::CreateDebuggerInfo( int iLineNo )
{
	minDebuggerInfo * pDebuggerInfo = new minDebuggerInfo();
	pDebuggerInfo->iLineNo = iLineNo;
	return minHandle<minDebuggerInfo>( pDebuggerInfo );
}

bool minInterpreterEnvironment::IsAtBreakpoint( minInterpreterNode * pCurrentNode ) const
{
	int iLineNo = pCurrentNode->GetLineNumber();
	return IsAtBreakpoint(iLineNo);
}

bool minInterpreterEnvironment::IsAtBreakpoint(int iLineNo) const
{
	BreakpointContainerT::const_iterator iter = m_aBreakpointContainer.begin();
	while (iter != m_aBreakpointContainer.end())
	{
		if ((*iter).iLineNo == iLineNo)
		{
			return true;
		}
		++iter;
	}
	return false;
}

list<int> minInterpreterEnvironment::GetBreakpointLines() const
{
	list<int> ret;
	BreakpointContainerT::const_iterator iter = m_aBreakpointContainer.begin();
	while( iter != m_aBreakpointContainer.end() )
	{
		ret.push_back( (*iter).iLineNo );
		++iter;
	}
	return ret;
}

static bool IsRealCallStackItem( const string & sInfo )
{
	return sInfo.length() > 0 && (sInfo[0] == '?' || sInfo.substr(0, 6) == "__main");
}

static string FillWithSpacesToLength(const string & s, int iLength, bool bFillRight)
{
	if ((int)s.length()<iLength)
	{
		return s + string(iLength - s.length(), ' ');
	}
	return s;
}

vector<string> minInterpreterEnvironment::GetCallStackForDebugger( const string & sSourceCode, const CallStackContainerT & aCallStack, int iSelectedCallStackLevel ) const
{
	vector<string> ret;

	char sBuffer[c_iMaxBuffer];
	int nCurrentLine = -1;
	int i = 1;
	int iMarkerPos = -1;
	bool markPos = false;

	vector<string> lines = split(sSourceCode, string("\n"));
	size_t n = m_aCallStack.size();
	CallStackContainerT::const_reverse_iterator iter = aCallStack.rbegin();
	while (iter != m_aCallStack.rend())
	{
		if (iSelectedCallStackLevel == n)
		{
			markPos = true;
		}

		string sInfo = (*iter)->GetInfoString();
		if( nCurrentLine<0 && sInfo.length() >= 5 && sInfo.substr(0, 5) == "block" )
		{
			nCurrentLine = (*iter)->GetCurrentLine();
		}
		if (IsRealCallStackItem(sInfo))
		{
			sInfo = FillWithSpacesToLength(sInfo, 40, true);

			sprintf(sBuffer, "%d", nCurrentLine);
			string sLineNo(" lineno=");
			sLineNo += sBuffer;
			sLineNo = FillWithSpacesToLength(sLineNo, 15, true);
			sInfo += sLineNo;

			sInfo += " module=";
			sInfo += GetFileNameFromScript(lines);
            
			//sprintf(sBuffer, "%d ", i);
			//sInfo = sBuffer + sInfo;

			sInfo = string("name=") + sInfo;

			if (markPos)
			{
				//sInfo = "-> " + sInfo;
				iMarkerPos = i;
				markPos = false;
			}
			else
			{
				//sInfo = "   " + sInfo;
			}

			nCurrentLine = -1;

			ret.push_back(sInfo);
			++i;
		}

		--n;
		++iter;
	}

	reverse(ret.begin(), ret.end());

	for (size_t j = 0; j < ret.size(); j++)
	{
		sprintf(sBuffer, "%zd ", j+1);
		ret[j] = sBuffer + ret[j];

		if (j==i-iMarkerPos-1)
		{
			ret[j] = "-->" + ret[j];
		}
		else
		{
			ret[j] = "   " + ret[j];
		}
	}

	//ret[i - iMarkerPos - 1] = "--> " + ret[i - iMarkerPos - 1];

	return ret;
}

pair<int,int> minInterpreterEnvironment::GetLineNoOfCallStackItem(const CallStackContainerT & aCallStack, int iSelectedCallStackLevel, int & iLevelDown, int & iLevelUp ) const
{
	bool bStart = false;
	bool bSelected = false;

	size_t n = m_aCallStack.size();
	int iLastRealCallstackLevel = -1;
	int iLineNo = -1;
	int iLastLineNo = -1;
	CallStackContainerT::const_reverse_iterator iter = aCallStack.rbegin();

// TODO: diese loop gibt es zweimal !!!
	while (iter != m_aCallStack.rend())
	{
		string sInfo = (*iter)->GetInfoString();

// operation 1)
		//int no = (*iter)->GetLine();
		
		// get line number of first block for real stack item
		if (iLineNo<0 && sInfo.length() >= 5 && sInfo.substr(0, 5) == "block")
		{
			iLineNo = (*iter)->GetCurrentLine();
		}
		

		if (!bStart)
		{
			bStart = true;
			bSelected = false;
		}
		if (n == iSelectedCallStackLevel)
		{
			bSelected = true;
		}
		if (IsRealCallStackItem(sInfo))
		{
			if (!bSelected)
			{
// operation 2)
				bStart = false;
				iLastLineNo = iLineNo;
				iLineNo = -1;
			}
			else
			{
// operation 3)
				iLevelDown = (int)n;
				iLevelUp = iLastRealCallstackLevel;
				return pair<int,int>(iLineNo, iLastLineNo);
			}

			iLastRealCallstackLevel = (int)n;
		}

		--n;
		++iter;
	}

	return pair<int,int>(-1,-1);
}

minCallStackItem::VariableContainerT minInterpreterEnvironment::GetVairablesForDebugger(const CallStackContainerT & aCallStack, int iSelectedCallStackLevel) const
{
	minCallStackItem::VariableContainerT aVariables;

	/*
  1 __main():
  2 block current_line=24: argc{int}=0
  3 ?main() call_line=24:
  4 block current_line=13: i{int}=0; j{int}=0; d{double}=0.000000
  5 block current_line=17: b{bool}=false
  6 ?f() call_line=17: x{double}=1.234000
->7 block current_line=4:
---------------------------
  3 __main():  current_line=24
  2 ?main() call_line=24:  current_line=17
->1 ?f() call_line=17: x{double}=1.234000 current_line=4
	
	iSelectedCallStackLevel		range
	7							7-6			Start = 7	selected=false	realCallStackFinished=false (reset with Start = ...)
	6							7-6						selected=true	realCallStackFinished=true
	5							5-3			Start = 5	if !selcted --> 
	4							5-3
	3							5-3
	2							2-1
	1							2-1

	start = false, selected = false //, realCallStackFinished = false
	loop n
		if !start:
			start = true
			selected = false
			//realCallStackFinished = false
		if n == iSelectedCallStackLevel:
			selected = true
		if isRealCallStackItem:
			//realCallStackFinished = true
			if !selected:
				aVariables.Clear()		// reset local variables container
				start = false
			else:
				return data

		data.Add(callstack[n])

	
	*/

	// collect all local variables in all "blocks" belonging to a real call stack item 

	bool bStart = false;
	bool bSelected = false;

	size_t n = aCallStack.size();
	CallStackContainerT::const_reverse_iterator iter = aCallStack.rbegin();
	while (iter != m_aCallStack.rend())
	{
		string sInfo = (*iter)->GetInfoString();

		const minCallStackItem::VariableContainerT & currentVariables = (*iter)->GetVariables();
		aVariables.insert(aVariables.end(), currentVariables.begin(), currentVariables.end());

		if (!bStart)
		{
			bStart = true;
			bSelected = false;
		}
		if( n == iSelectedCallStackLevel )
		{
			bSelected = true;
		}
		if( IsRealCallStackItem( sInfo ) )
		{
			if( !bSelected )
			{
				aVariables.clear();
				bStart = false;
			}
			else
			{
				return aVariables;
			}
		}

		--n;
		++iter;
	}

	return aVariables;
}

/*
// see: http://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
inline std::string trim(const std::string &s)
{
	string::const_iterator wsfront = std::find_if_not(s.begin(), s.end(), [](int c){return std::isspace(c, std::locale::classic()); });
	string::const_iterator wsback = std::find_if_not(s.rbegin(), s.rend(), [](int c){return std::isspace(c, std::locale::classic()); }).base();
	return (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));
}
*/

// see: http://stackoverflow.com/questions/25829143/c-trim-whitespace-from-a-string
static string trim(const string& str)
{
	if( str.length()>0 )
	{
		size_t first = str.find_first_not_of(' ');
		size_t last = str.find_last_not_of(' ');
		if( first==/*SIZE_MAX*/(size_t)-1 && first==last )
		{
			return string();
		}
		return str.substr(first, (last - first + 1));
	}
	return string();
}

//// see: https://www.safaribooksonline.com/library/view/c-cookbook/0596007612/ch04s07.html
//void split( const string & s, char c, vector<string> & v )
//{
//	string::size_type i = 0;
//	string::size_type j = s.find(c);
//
//	while (j != string::npos) {
//		v.push_back(s.substr(i, j - i));
//		i = ++j;
//		j = s.find(c, j);
//
//		if (j == string::npos)
//			v.push_back(s.substr(i, s.length()));
//	}
//}
//
//// see: http://stackoverflow.com/questions/236129/split-a-string-in-c
//void splitx( const string & s, char delim, vector<string> & elems ) 
//{
//	stringstream ss;
//	ss.str(s);
//	string item;
//	while (getline(ss, item, delim)) 
//	{
//		elems.push_back(item);
//	}
//}
//
//vector<string> split(const string &s, char delim)
//{
//	vector<string> elems;
//	split(s, delim, elems);
//	return elems;
//}

//// see: http://stackoverflow.com/questions/236129/split-a-string-in-c
//void tokenize( const string & str, vector<string> & tokens, const string& delimiters = " ", bool trimEmpty = false )
//{
//	string::size_type pos, lastPos = 0, length = str.length();
//
//	while( lastPos < length + 1 )
//	{
//		pos = str.find_first_of(delimiters, lastPos);
//		if( pos == std::string::npos )
//		{
//			pos = length;
//		}
//
//		if( pos != lastPos || !trimEmpty )
//		{
//			string s = trim(string(vector<string>::value_type(str.data() + lastPos, (vector<string>::size_type)pos - lastPos)));
//			if( s.length()>0 )
//			{
//				tokens.push_back(s);
//			}
//		}
//
//		lastPos = pos + 1;
//	}
//}

bool minInterpreterEnvironment::ProcessDbg( minInterpreterNode * pCurrentNode )
{
	int nCurrentLineNo = pCurrentNode->GetLineNumber();

// TODO: visiscript: unterschiedliche Farben im Ausgabe-Fenster fuer stdout und stderr ?!

// ((TODO: korrekte Zeilennummern wie im original Script verwenden --> Whitespace tokens nicht ignorieren...?
// ((TODO: r --> return from function
// ((TODO: p --> restart program 
// ((TODO: restart program execution after finishing program
// ((TODO: bei Exceptions anhalten und callstack anzeigen
// TODO: optimierung: in FunctionNode.DoExecute() ist der Aufruf GetNoOfFunctions() teuer ist der wirklich notwendig ? GetFunction() ist ebenfalls teuer
// TODO: optimierung: in minVariableNode::DoExecute() ist GetValueForVariable() und CopyValue() teuer !
// TODO: ggf. maps/dictionaries als einfachen datentyp implementieren?
// TODO: ggf. dynamic type variablen typ definieren --> var aVariant = 4; aVariang = "blub"
// TODO: ggf. evaluation von Ausdruecken im debug mode erlauben: i*i oder 9*9
// TODO: ggf. len(array) implementieren
// TODO: ggf. bei Breakpoints stoppen, fuer die kein code existiert, z. b. block anfang/ende --> ignoriere Breakpoints fuer leerzeilen und kommentare ?
	// TODO: aussagekraeftige Fehlermeldungen realisieren --> strings fuer parser fehler codes !
	// ((TODO: zeilen nummern korrekt zurueckgeben --> offset bestimmen
	// TODO: navigate in callstack: up, down
	// TODO: pruefe, ob breakpoint ueberhaupt gesetzt werden kann --> gibt es InterpreterNode fuer gesuchte Zeilennummer?
// ((TODO: bug untersuchen mit do not halt on breakpoint line --> problem ist mehrzeiliger c kommentar ! /* */ --> Zeilennummer des Token wurde nicht korrekt hoch gezaehlt !
// ((TODO: abbruch bei erstem Parserfehler und nicht alle folge Fehler anzeigen !
// TODO: Content von Objekten und Arrays anzeigen
// TODO: lb --> list of breakpoints
// TODO: cl n --> clear breakpoint at line n
// TODO: a --> dump AST fuer ganzes script
// TODO: dump() auch fuer ForNode, etc. implementieren...
// TODO: was ist mit dem block-Eintrag auf dem Stack, ist der wirklich notwendig?
// TODO: minscript debugger ausgaben auf stderr ausgeben?
// TODO: minscript richtung clean code aufraeumen, grosse methoden verkleinern...

	if( m_bDbgResetExecution )
	{
		return false;
	}

// TODO --> breakpoint behandlung --> zeilen nummern beachten ! alle statements der breakpoint zeile abarbeiten !
	bool bIsAtBreakpoint = IsAtBreakpoint( nCurrentLineNo );
	if (bIsAtBreakpoint)
	{
		if (m_nLastBreakpointLineNo != nCurrentLineNo)
		{
			//cout << "Hit breakpoint !" << endl;
			m_nLastBreakpointLineNo = nCurrentLineNo;
		}
		else
		{
			bIsAtBreakpoint = false;
		}
	}
	else
	{
		m_nLastBreakpointLineNo = 0;
	}

// TODO --> behandeln, dass an der naechsten anweisung gestoppt wird (statement)
//	bool bContinueUntilNextLine = m_iNextStepLineNo;

    if( !bIsAtBreakpoint && m_bRunDbg && !HasError() )
    {
        return true;
    }

	size_t iCurrentDbgCallStack = GetCallStackForDebugger(m_sSourceCode, m_aCallStack, -1).size();

    // process step over next line
	if (m_bStepToNextLine && !bIsAtBreakpoint && !HasError() && 
		/* special handling for first step over in program, jump into first meta block */!(m_aCallStack.size() == 2 && m_nCurrentCallStackLevel == 1) &&
        ( nCurrentLineNo==m_nCurrentLineNo || nCurrentLineNo==/*ignore*/0 ||
		(m_nCurrentDebuggerCallStackLevel>0 && iCurrentDbgCallStack>m_nCurrentDebuggerCallStackLevel)
          /*(m_nCurrentCallStackLevel>0 && m_aCallStack.size()>(CallStackContainerT::size_type)m_nCurrentCallStackLevel)*/ ) )
    {
        //cout << "cont. over " << m_nCurrentCallStackLevel << " " << m_aCallStack.size() << " brkpnt=" << bIsAtBreakpoint << endl;
        return true;
    }

    // process step into next line
    if (m_bStepIntoNextLine && !bIsAtBreakpoint && !HasError() &&
        ( nCurrentLineNo==m_nCurrentLineNo || nCurrentLineNo==/*ignore*/0 ) )
    {
		//cout << "cont. into " << m_nCurrentCallStackLevel << " " << m_aCallStack.size() << " brkpnt=" << bIsAtBreakpoint << endl;
        return true;
    }

	// process step out (return from function)
    if (m_bStepOut && !bIsAtBreakpoint && !HasError() &&
		(nCurrentLineNo == m_nCurrentLineNo || nCurrentLineNo ==/*ignore*/0 ||
		(m_nCurrentCallStackLevel>0 && m_aCallStack.size()>(CallStackContainerT::size_type)m_nCurrentCallStackLevel-1)))
	{
		//cout << "cont. out " << m_nCurrentCallStackLevel << " " << m_aCallStack.size() << " brkpnt=" << bIsAtBreakpoint << endl;
		return true;
	}

// TODO --> debugger interface implementieren: fuer anzeige Quellcode, etc.
//          oder: debugger extern in eigener Klasse implementieren und das InterpreterEnvironment und den node hineinreichen... ?

// TODO --> script-name und line-no an token / interpreterNode dran haengen
    //cout << pCurrentNode->GetClassName() << " " << pCurrentNode->GetInfo() << endl;

    //cout << "Line number=" << nCurrentLineNo << " stacksize: " << m_aCallStack.size() << endl;

	DumpScript(m_sSourceCode, m_nLineCountOfAddedCode, nCurrentLineNo, GetBreakpointLines(), true);

    //pCurrentNode->Dump( cout );

// TODO --> user input request, alle zustaende zuruecksetzen...

	// get real call stack level from helper call stack level
	size_t iSelectedCallStackLevel = m_aCallStack.size();
	int iLevelDown, iLevelUp;
	/*nCurrentLineNo =*/ GetLineNoOfCallStackItem(m_aCallStack, (int)iSelectedCallStackLevel, iLevelDown, iLevelUp);
	iSelectedCallStackLevel = iLevelDown;

    bool bContinueDbgLoop = true;
    while( bContinueDbgLoop )
    {
        cout << /*endl <<*/ "(mdb) > ";
        string sInput;
		getline( cin, sInput );
		sInput = trim(sInput);
        //cin >> sInput;
        if( sInput=="n" )   // step next ast
        {
            //cout << "next AST step" << endl;

			ResetDebuggerExecutionFlags();

			bContinueDbgLoop = false;
		}
		else if (sInput == "v" || sInput == "over")   // step next line
        {
            //cout << "step over next line" << endl;

// ((TODO --> alle nodes fuer eine Zeile ausfuehren... --> zeilennummer merken und solange ausfuehren bis eine andere zeilennummer kommt !
// TODO --> block item in callstack notwendig?
			ResetDebuggerExecutionFlags();
			m_bStepToNextLine = true;

            m_nCurrentLineNo = nCurrentLineNo;
            m_nCurrentCallStackLevel = (int)m_aCallStack.size();
			m_nCurrentDebuggerCallStackLevel = (int)GetCallStackForDebugger(m_sSourceCode, m_aCallStack, (int)iSelectedCallStackLevel).size();

			bContinueDbgLoop = false;
		}
		else if (sInput == "s" || sInput == "step")   // step into next line
        {
            //cout << "step into next line" << endl;

			ResetDebuggerExecutionFlags();
			m_bStepIntoNextLine = true;

            m_nCurrentLineNo = nCurrentLineNo;
            m_nCurrentCallStackLevel = (int)m_aCallStack.size();
			m_nCurrentDebuggerCallStackLevel = (int)GetCallStackForDebugger(m_sSourceCode, m_aCallStack, (int)iSelectedCallStackLevel).size();

			bContinueDbgLoop = false;
		}
		else if (sInput == "o" || sInput == "out")   // step out
		{
			//cout << "step out" << endl;

			ResetDebuggerExecutionFlags();
			m_bStepOut = true;

			m_nCurrentLineNo = nCurrentLineNo;
			m_nCurrentCallStackLevel = (int)m_aCallStack.size();
			m_nCurrentDebuggerCallStackLevel = (int)GetCallStackForDebugger(m_sSourceCode, m_aCallStack, (int)iSelectedCallStackLevel).size();

			bContinueDbgLoop = false;
		}
		else if (sInput == "r" || sInput == "run")
        {
            //cout << "run..." << endl;

			ResetDebuggerExecutionFlags();
			m_bRunDbg = true;

			bContinueDbgLoop = false;
		}
		else if (sInput == "restart")
		{
			//cout << "reset execution..." << endl;

			ResetDebuggerExecutionFlags();
			m_bRunDbg = true;
			m_bDbgResetExecution = true;

			bContinueDbgLoop = false;
		}
		else if (sInput == "d" || sInput == "down")
		{
			if( iSelectedCallStackLevel > 1 )
			{
				// jump only real call stack items and ignore the virtual call stack items "blocks" !
				iSelectedCallStackLevel--;
				pair<int,int> result = GetLineNoOfCallStackItem( m_aCallStack, (int)iSelectedCallStackLevel, iLevelDown, iLevelUp );
				nCurrentLineNo = result.first;
				iSelectedCallStackLevel = iLevelDown;
			}
			else
			{
				cout << "Warning: can not move callstack down!" << endl;
			}
		}
		else if (sInput == "u" || sInput == "up")
		{
			bool bCanNotMove = false;
			if( iSelectedCallStackLevel < (int)m_aCallStack.size() )
			{
				// jump only real call stack items and ignore the virtual call stack items "blocks" !
				iSelectedCallStackLevel++;
				pair<int, int> result = GetLineNoOfCallStackItem(m_aCallStack, (int)iSelectedCallStackLevel, iLevelDown, iLevelUp);
				nCurrentLineNo = result.second;
				if (iLevelUp < 0)
				{
					bCanNotMove = true;
					iSelectedCallStackLevel--;
				}
				else
				{
					iSelectedCallStackLevel = iLevelUp;
				}
			}
			else
			{
				bCanNotMove = true;
			}
			if( bCanNotMove )
			{
				cout << "Warning: can not move callstack up!" << endl;
			}
		}
		else if (sInput == "k" || sInput == "stack")
        {
#ifdef _DEBUGGING_DEBUGGER
			cout << "show stack size=" << GetCallStackSize() << endl;
// TODO --> Transformation: funktionen und Blocks zusammenfassen zu einem debugger callstack item !?
            CallStackContainerT::const_iterator iter = m_aCallStack.begin();
            int i = 1;
			int iMaxCallStackItems = m_aCallStack.size();
            while( iter!=m_aCallStack.end() )
            {
				if( iSelectedCallStackLevel==i )
				{
					cout << "->";
				}
				else
				{
					cout << "  ";
				}
				//pCurrentNode->GetLineNumber()
				cout << i << " " << (*iter)->GetInfoString() << endl;
                ++iter;
                ++i;
            }

#endif
			//cout << "show stack:"  << endl;
			//cout << "-----------" << endl;

			vector<string> aDbgCallStack = GetCallStackForDebugger(m_sSourceCode, m_aCallStack, (int)iSelectedCallStackLevel);
			for (size_t n = 0; n<aDbgCallStack.size(); n++)
			{
				cout << aDbgCallStack[n] << endl;
			}
        }
		else if (sInput == "l" || sInput == "locals")
		{
#ifdef _DEBUGGING_DEBUGGER
			//minHandle<minCallStackItem> aCurrentCallStackItem = GetActCallStackItem();
			minHandle<minCallStackItem> aCurrentCallStackItem = GetCallStackItemForLevel( iSelectedCallStackLevel );
			cout << "local variables: " << aCurrentCallStackItem->GetInfoString() << endl;
			aCurrentCallStackItem->DumpVariables(cout);

#endif
			//cout << "show local variables:" << endl;
			//cout << "---------------------" << endl;

			minCallStackItem::VariableContainerT temp = GetVairablesForDebugger(m_aCallStack, (int)iSelectedCallStackLevel);
			minCallStackItem::VariableContainerT::iterator iter = temp.begin();
			while (iter != temp.end())
			{
				streamsize old = cout.width(20);
				cout << (*iter).GetName();
				cout.width(old);
				cout << " --> ";
				cout << left;
				old = cout.width(40);
				cout << (*iter).GetValue()->GetString(true);
				cout.width(old);
				cout << right;
				cout << " : " << (*iter).GetValue()->GetTypeString() << endl;
				++iter;
			}
		}
		else if( sInput.length()>1 && sInput[0]=='b' && sInput[1]==' ' )
		{
// TODO --> DbgInfo an minInterpreterNode setzen, Struktur mit lineNo, FileName --> Diese Debug Infos ggf. auch in anderem Container verwalten?
// TODO --> pruefe ob es einen Interpreter-Knoten fuer diese Zeilennummer ueberhaupt gibt --> falls nein --> ablehen und Fehlermeldung
			string::size_type iSeparatorPos = sInput.find_last_of(':');
			string sFileName;
			string sLineNo;
			string sCondition;
			string sRest;

			if (iSeparatorPos != string::npos)
			{
				// process something like: b c:\path\filename.ic:42 --> found : before 42
				sFileName = trim(sInput.substr(2, iSeparatorPos - 2));
				sRest = sInput.substr(iSeparatorPos + 1);
			}
			else
			{
				// process something like: b 42 [cond]
				sRest = sInput.substr(2);
			}

			string::size_type iSpacePos = sRest.find_first_of(' ');
			if (iSpacePos != string::npos)
			{
				sLineNo = trim(sRest.substr(0, iSpacePos));
				sCondition = trim(sRest.substr(iSpacePos + 1));
			}
			else
			{
				sLineNo = trim(sRest);
			}

			int iBreakOnLine = -1;	// not defined
			if (sLineNo.length() > 0)
			{
				iBreakOnLine = atoi(sLineNo.c_str());
			}

			minBreakpointInfo aBreakpoint;
			if (iBreakOnLine > 0)
			{
				aBreakpoint.iLineNo = iBreakOnLine;
				aBreakpoint.sFileName = sFileName;
				aBreakpoint.sCondition = sCondition;
// TODO: check if breakpoint for line already exists --> overwrite !
// TODO: behandlung von breakpoints in files realisieren
// TODO: behandlung von conditions in breakpoints realisieren
				m_aBreakpointContainer.push_back(aBreakpoint);
				cout << "set breakpoint at line " << iBreakOnLine << " in file: ";
				if( sFileName.length()>0 )
				{
					cout << sFileName;
				}
				else
				{
					cout << "<mainscript>";
				}
				if( sCondition.size() )
				{
					cout << " with condition: " << sCondition << endl;
				}
				//cout << " number total breakpoints=" << m_aBreakpointContainer.size() << endl;
				cout << endl;
			}
			else
			{
				cout << "Warning: breakpoint not set !" << endl;
			}
		}
		else if (sInput == "t" || sInput == "list")
		{
			int i = 1;
			BreakpointContainerT::const_iterator iter = m_aBreakpointContainer.begin();
// TODO enabled behandeln 
			cout << "  No LineNo            FileName Condition" << endl;
			cout << "-------------------------------------------" << endl;
			while (iter != m_aBreakpointContainer.end())
			{
				minBreakpointInfo info = *iter;

				streamsize old = cout.width(4);
				cout << right;
				cout << i;
				cout.width(old);
				cout << " ";
				cout.width(6);
				cout << info.iLineNo;
				cout.width(20);
				cout << info.sFileName;
				cout.width(old);
				cout << " " << info.sCondition << endl;

				++iter;
				++i;
			}
		}
		else if (sInput == "clear")
		{
			cout << "Really delete all breakpoints? (y/n)" << endl;
			string sUserInput;
			while (!(sUserInput == "y" || sUserInput == "n"))
			{
				getline(cin, sUserInput);
			}
			if (sUserInput=="y")
			{
				size_t iSize = m_aBreakpointContainer.size();
				m_aBreakpointContainer.clear();
				//cout << "Cleared " << iSize << " breakpoints" << endl;
			}
		}
		else if (sInput == "a")
		{
		    SMALL( pCurrentNode->Dump( cout ); )
			//DumpParser(cout);
		}
		else if (sInput == "c" || sInput == "code")
        {
// TODO: dump von anderen script files: s header.ic
			DumpScript( m_sSourceCode, m_nLineCountOfAddedCode, nCurrentLineNo, GetBreakpointLines() );
        }
		else if (sInput == "version")
		{
			SMALL( DumpVersion(cout); ) 
		}
		else if (sInput == "e" || sInput == "exit" )
        {
            cout << "exit debugging..." << endl;
            exit(-1);
        }
        else if( sInput == "h" || sInput == "help" )
        {
			cout << endl;
            cout << "help for debugger:" << endl << endl;
			cout << "  (h)elp                     : show help" << endl;
			cout << "  version                    : show interpreter version" << endl;
			cout << "  (c)ode                     : show source code" << endl;
			cout << "  stac(k)                    : show call stack" << endl;
			cout << "  (u)p                       : one step up the call stack" << endl;
			cout << "  (d)own                     : one step down the call stack" << endl;
			cout << "  (r)un                      : run/continue" << endl;
			cout << "  (s)tep                     : step into next line" << endl;
			cout << "  o(v)er                     : step over next line" << endl;
			cout << "  (o)ut                      : step out (return from function)" << endl;
			cout << "  b [filename:]lineno [cond] : set breakpoint at line" << endl;		
// TODO 
			// b lineno;filename 
			// b lineno filename
//			cout << "  clear [no]                 : clears a breakpoint with no or all beakpoints" << endl;
			cout << "  clear                      : clear all breakpoins" << endl;
//			cout << "  t                          : toggle breakpoint at line" << endl;
//			cout << "  e lineno                   : erase breakpoint at line" << endl;
// TODO done
			cout << "  lis(t)                     : show all breakpoints" << endl;
			cout << "  restart                    : reset/restart program execution" << endl;
			cout << "  (l)ocals                   : show local variables" << endl;
			cout << "  a                          : show AST" << endl;
			cout << "  n                          : next AST step" << endl;
			cout << "  (e)xit                     : exit debugging" << endl;
			cout << endl;
        }
        else
        {
            cout << "Unknown input: " << sInput << endl;
        }
    }

	return true;
}

void minInterpreterEnvironment::Dump() const
{
	cout << "Callstack:" << endl;
	cout << GetInfoString().c_str();
	cout << "done." << endl << endl;;
}

minHandle<minCallStackItem> minInterpreterEnvironment::GetLatestVisibleObjectOnStack( int iStackDelta ) const
{
	// Variable in allen Call-Stack Eintragen (von der Spize an) suchen
	CallStackContainerT::const_reverse_iterator aIter = m_aCallStack.rbegin();

	int iCount = 0;
	while( (iCount<iStackDelta) && (aIter != m_aCallStack.rend()) )
	{
		++aIter;
		++iCount;
	}

	while( aIter != m_aCallStack.rend() )
	{
		minHandle<minCallStackItem> hCallStackItem = *aIter;
		if( hCallStackItem->IsObject() )
		{
			return hCallStackItem;
		}
		// der Callstack darf nicht durch einfache Funktionen
		// durchbrochen sein, d.h. durch einfache Funktionen
		// wird das Objekt fuer einen Aufrufer unsichtbar,
		// zumindest was den Scope fuer einen Methoden-Call
		// betrifft.
		if( hCallStackItem->HidesObject() )
		{
			break;
		}
		++aIter;
	}

	return minHandle<minCallStackItem>();	// nicht gefunden
}

string minInterpreterEnvironment::GetLatestClassScopeForLatestObject() const
{
	string sClassName;

	CallStackContainerT::const_reverse_iterator aIter = m_aCallStack.rbegin();

	while( aIter != m_aCallStack.rend() )
	{
		minHandle<minCallStackItem> hCallStackItem = *aIter;

		const string & sItemName = hCallStackItem->GetItemName();

		if( (sItemName.length()>1) && (sItemName.substr(0,strlen(g_sFunctionCallStart))==g_sFunctionCallStart) )
		{
			size_t iFound = sItemName.find( g_sClassMethodSeparator );
			if( iFound != string::npos )
			{
				return sItemName.substr( 1, iFound-1 );
			}
		}

		// Bugfix 5.2.2003
		if( hCallStackItem->IsObject() )
		{
			return hCallStackItem->GetUserName();
		}
/*
		if( hCallStackItem->IsObject() )
		{
			return hCallStackItem;
		}
		// der Callstack darf nicht durch einfache Funktionen
		// durchbrochen sein, d.h. durch einfache Funktionen
		// wird das Objekt fuer einen Aufrufer unsichtbar,
		// zumindest was den Scope fuer einen Methoden-Call
		// betrifft.
		if( hCallStackItem->HidesObject() )
		{
			break;
		}
*/
		++aIter;
	}

	return sClassName;
}

minHandle<minInterpreterValue> minInterpreterEnvironment::GetValueForVariable( const string & sName )
{
	// Variable in allen Call-Stack Eintragen (von der Spize an) suchen
	CallStackContainerT::reverse_iterator aIter = m_aCallStack.rbegin();

	while( aIter != m_aCallStack.rend() )
	{
		minHandle<minCallStackItem> hCallStackItem = *aIter;
		minHandle<minInterpreterValue> hValue = hCallStackItem->GetValueForVariable( sName );
		if( hValue.IsOk() )
			return hValue;						// gefunden !
		++aIter;
	}
	return minHandle<minInterpreterValue>();	// nicht gefunden
}

bool minInterpreterEnvironment::AddNewVariable( const string & sName, minHandle<minInterpreterValue> hValue )
{
	// Variable im aktuellen Call-Stack Element anlegen,
	// diese Methode wird benoetigt um Variablen ggf. durch Zuweisung
	// automatisch anzulegen !!! (z.B. i = 0;)
	minHandle<minCallStackItem> hActItem = GetActCallStackItem();
	if( hActItem.IsOk() )
	{
		return hActItem->AddNewVariable( sName, hValue );
	}
	return false;
}

class CompareFunctions
{
public:
	// < Operator neu definieren fuer diesen Vergleich
	bool operator()( const minHandle<minFunctionDeclarationNode> hLeft, const minHandle<minFunctionDeclarationNode> hRight ) const 
	{
		return hLeft->GetName() < hRight->GetName();
	}
};

int minInterpreterEnvironment::GetNoOfFunctions( const string & sName ) const
{
	int nCount = 0;

#ifdef _old_name_search
	FunctionContainerT::const_iterator aFuncIter = m_aFunctionContainer.begin();

	while( aFuncIter != m_aFunctionContainer.end() )
	{
		minHandle<minFunctionDeclarationNode> hFunction = *aFuncIter;
		if( hFunction->GetName() == sName )
		{
			++nCount;	// gefunden !
		}
		++aFuncIter;
	}
#else
    minHandle<minFunctionDeclarationNode> hTemp( new minInterpreterFunctionDeclarationNode( sName, false, false, Void, minVariableDeclarationList(), 0, minParserItemList(), /*TODO DEBUG*/0 ) );

	pair<FunctionContainerT::const_iterator,FunctionContainerT::const_iterator> aFound 
			= equal_range( m_aFunctionContainer.begin(), m_aFunctionContainer.end(), hTemp, CompareFunctions() );

	FunctionContainerT::const_iterator aIter = aFound.first;
	while( aIter != aFound.second )
	{
		++nCount;
		++aIter;
	}
#endif
	return nCount;
	//ggf. return count( m_aFunctionContainer.begin(), m_aFunctionContainer.end(), sName );
}

minHandle<minFunctionDeclarationNode> minInterpreterEnvironment::GetFunction( const string & sName ) const
{
#ifdef _old_name_search
	FunctionContainerT::const_iterator aFuncIter = m_aFunctionContainer.begin();

	while( aFuncIter != m_aFunctionContainer.end() )
	{
		minHandle<minFunctionDeclarationNode> hFunction = *aFuncIter;
		if( hFunction->GetName() == sName )
		{
			return hFunction;	// gefunden !
		}
		++aFuncIter;
	}
#else
    minHandle<minFunctionDeclarationNode> hTemp( new minInterpreterFunctionDeclarationNode( sName, false, false, Void, minVariableDeclarationList(), 0, minParserItemList(), /*TODO DEBUG*/0 ) );

	pair<FunctionContainerT::const_iterator,FunctionContainerT::const_iterator> aFound 
			= equal_range( m_aFunctionContainer.begin(), m_aFunctionContainer.end(), hTemp, CompareFunctions() );

	if( (aFound.first != aFound.second) && (aFound.first != m_aFunctionContainer.end()) )
	{
		return *(aFound.first);
	}
#endif
	return minHandle<minFunctionDeclarationNode>( 0 );		// nicht gefunden --> liefert !IsOk()
}

minHandle<minFunctionDeclarationNode> minInterpreterEnvironment::GetFunctionForManglingName( const string & sName, const string & sManglingName ) const
{
#ifdef _old_name_search
	FunctionContainerT::const_iterator aFuncIter = m_aFunctionContainer.begin();

	while( aFuncIter != m_aFunctionContainer.end() )
	{
		minHandle<minFunctionDeclarationNode> hFunction = *aFuncIter;
		if( (hFunction->GetName()==sName) && IsManglingNameCompatible( hFunction->GetManglingName(), sManglingName ) )
		{
			return hFunction;	// gefunden !
		}
		++aFuncIter;
	}
#else
	// temporare Funktion anlegen, notwendig zur Suche
    minHandle<minFunctionDeclarationNode> hTemp( new minInterpreterFunctionDeclarationNode( sName, false, false, Void, minVariableDeclarationList(), 0, minParserItemList(), /*TODO DEBUG*/0 ) );

	pair<FunctionContainerT::const_iterator,FunctionContainerT::const_iterator> aFound 
			= equal_range( m_aFunctionContainer.begin(), m_aFunctionContainer.end(), hTemp, CompareFunctions() );

	// gefundene Funktionen noch anhand des Mangling-Namens unterscheiden...
	FunctionContainerT::const_iterator aIter = aFound.first;
	while( aIter != aFound.second )
	{
		minHandle<minFunctionDeclarationNode> hFunction = *aIter;
		
		if( IsManglingNameCompatible( hFunction->GetManglingName(), sManglingName ) )
		{
			return hFunction;
		}

		++aIter;
	}
#endif
	return minHandle<minFunctionDeclarationNode>( 0 );		// nicht gefunden --> liefert !IsOk()
}

bool minInterpreterEnvironment::AddFunction( minHandle<minFunctionDeclarationNode> hFunction )
{
	// existiert schon eine Funktion mit dem gleichen Namen ? (19.12.1999)
	minHandle<minFunctionDeclarationNode> aFcnWithSameName = GetFunctionForManglingName( hFunction->GetName(), hFunction->GetManglingName() );
	if( aFcnWithSameName.IsOk() )
	{
		return false;
	}

#ifdef _old_name_search
	m_aFunctionContainer.push_back( hFunction );
#else
	// neue Funktion sortiert einfuegen
	FunctionContainerT::iterator aFound = upper_bound( m_aFunctionContainer.begin(), m_aFunctionContainer.end(), hFunction, CompareFunctions() );
	m_aFunctionContainer.insert( aFound, hFunction );
#endif
	return true;
}

bool minInterpreterEnvironment::AddNativeFunction( NativeFcnWrapperBase * pNativeFunc )
{
	minNativeFcnWrapperBaseAdapter * pFcn = new minNativeFcnWrapperBaseAdapter( pNativeFunc );
	return AddFunction( minHandle<minFunctionDeclarationNode>( new minNativeFunctionDeclarationNode( pFcn ) ) );
}

bool minInterpreterEnvironment::RemoveFunction( minHandle<minFunctionDeclarationNode> hFunction )
{
	m_aFunctionContainer.remove( hFunction );
	return true;
}

void minInterpreterEnvironment::RemoveAllFunctions()
{
	// ACHTUNG: das Environment ist NICHT Eigentuemer an den Funktionen, 
	//			diese werden von dem Programm-Block-Node verwaltet!!!
	//			D.h. hier NUR die Referenz auf den Zeiger loeschen !!!
	// siehe: minBlockNode::~minBlockNode()

	/* ALTER FEHLERHAFTER CODE bis 20.3.1999
	// jetzt noch alle Funktionen loeschen
	FunctionContainerT::iterator aFuncIter = m_aFunctionContainer.begin();

	while( aFuncIter != m_aFunctionContainer.end() )
	{
		minFunctionDeclarationNode * pFunction = *aFuncIter;
		delete pFunction;
		++aFuncIter;
	}
	*/
	m_aFunctionContainer.erase( m_aFunctionContainer.begin(), m_aFunctionContainer.end() );
}

void minInterpreterEnvironment::DumpAllFunctions( ostream & aStream ) const
{
	FunctionContainerT::const_iterator aFuncIter = m_aFunctionContainer.begin();

	while( aFuncIter != m_aFunctionContainer.end() )
	{
		minHandle<minFunctionDeclarationNode> hFunction = *aFuncIter;

		aStream << hFunction->GetName().c_str() << " args=" << hFunction->GetManglingName().c_str() << endl;

		++aFuncIter;
	}
}

void minInterpreterEnvironment::DumpAllFunctionPrototypes( ostream & aStream ) const
{
	FunctionContainerT::const_iterator aFuncIter = m_aFunctionContainer.begin();

	while( aFuncIter != m_aFunctionContainer.end() )
	{
		minHandle<minFunctionDeclarationNode> hFunction = *aFuncIter;

		aStream << hFunction->GetPrototypeString() << endl;

		++aFuncIter;
	}
}

minHandle<minClassDeclarationNode> minInterpreterEnvironment::GetClass( const string & sName ) const
{
	ClassContainerT::const_iterator aIter = m_aClassContainer.begin();

	while( aIter != m_aClassContainer.end() )
	{
		minHandle<minClassDeclarationNode> hClass = *aIter;
		if( hClass->GetName() == sName )
		{
			return hClass;
		}
		++aIter;
	}
	return minHandle<minClassDeclarationNode>( 0 );		// nicht gefunden --> liefert !IsOk()
}

bool minInterpreterEnvironment::AddClass( minHandle<minClassDeclarationNode> hClass )
{
	m_aClassContainer.push_back( hClass );
	return true;
}

bool minInterpreterEnvironment::RemoveClass( minHandle<minClassDeclarationNode> hClass )
{
	m_aClassContainer.remove( hClass );
	return true;
}

void minInterpreterEnvironment::RemoveAllClasses()
{
	m_aClassContainer.erase( m_aClassContainer.begin(), m_aClassContainer.end() );
}

minInterpreterType minInterpreterEnvironment::ResolveTypeFromString( const string & sTypeString, int iStackDelta ) const
{
	if( sTypeString==_VOID_MS )
		return minInterpreterType( Void );
	else if( sTypeString==_BOOL )
		return minInterpreterType( Bool );
	else if( sTypeString==_STRING_MS )
		return minInterpreterType( String );
	else if( sTypeString==_CHAR || sTypeString==_UCHAR )
		return minInterpreterType( CharTT );
	else if( sTypeString==_ULONG || sTypeString==_USHORT || sTypeString==_UINT ||
			 sTypeString==_LONG || sTypeString==_SHORT || sTypeString==_INT )
		return minInterpreterType( Int );
	else if( sTypeString==_FLOAT || sTypeString==_DOUBLE )
		return minInterpreterType( Double );
	else if( sTypeString==_FUNCTION )
		return minInterpreterType( Function );	
	else if( sTypeString.length()>0 )		// alles andere wird als Objekt interpretiert !
	{
		// existiert eine Klasse mit dem uebergebenen Namen oder ist es ggf ein Template-Type
		minHandle<minClassDeclarationNode> hClass = GetClass( sTypeString );

		if( !hClass.IsOk() )
		{
			// es muss ein Template-Type sein, bestimme den echten Template-Type
			minHandle<minCallStackItem> hObj = GetLatestVisibleObjectOnStack( iStackDelta );
			if( hObj.IsOk() )
			{
				minHandle<minClassDeclarationNode> hClass = GetClass( hObj->GetUserName() );
				if( hClass.IsOk() )
				{
					assert( hObj->GetCreator() != 0 );
					minInterpreterType aType = hClass->GetTemplateType( sTypeString, hObj->GetCreator()->GetAllTemplateTypes(), *this, iStackDelta );
					return aType;
				}
			}
			
			// ein Fehler in der Template-Typ Behandlung aufgetreten
			assert( false );
			return minInterpreterType( Unknown );
		}

		// es wurde eine Klasse mit dem angegebenen Namen gefunden !
		return minInterpreterType( Object, 0, false, sTypeString );
	}
	else
	{
		return minInterpreterType( Unknown );
	}
}

bool minInterpreterEnvironment::CheckForTemplateType( minInterpreterType & aType ) const
{
	if( aType.GetType() == TemplateType )
	{
		aType = ResolveTypeFromString( aType.GetTypeString() );
/*
		// Fuer Templates wird hier der korrekte Typ bestimmt !
		minHandle<minCallStackItem> hObj = GetLatestVisibleObjectOnStack();
		minHandle<minClassDeclarationNode> hClass = GetClass( hObj->GetUserName() );
		aType = hClass->GetTemplateType( aType.GetTypeString(), hObj->GetCreator()->GetAllTemplateTypes(), *this );
*/
		return true;
	}
	return false;	// uebergebener Typ war gar kein Template-Type !
}

//*************************************************************************

static bool g_bDebug = false;

bool IsDebugOn()
{
	return g_bDebug;
}

void SetDebugModus( bool bOn )
{
	g_bDebug = bOn;
}

ostream & GetDebugStream()
{
	return cerr;
}

const char * GetMethodSeparatorStrg()
{
	return g_sClassMethodSeparator;
}
/*
InterpreterValueType GetTypeFromString( const string & aTypeString )
{
	if( aTypeString==_VOID_MS )
		return Void;
	else if( aTypeString==_BOOL )
		return Bool;
	else if( aTypeString==_STRING_MS )
		return String;
	else if( aTypeString==_CHAR || aTypeString==_UCHAR )
		return CharTT;
	else if( aTypeString==_ULONG || aTypeString==_USHORT || aTypeString==_UINT ||
			 aTypeString==_LONG || aTypeString==_SHORT || aTypeString==_INT )
		return Int;
	else if( aTypeString==_FLOAT || aTypeString==_DOUBLE )
		return Double;
	else if( aTypeString==_FUNCTION )
		return Function;	
	else if( aTypeString.length()>0 )		// alles andere wird als Objekt interpretiert !
		return Object;
	else
		return Unknown;
}
*/
