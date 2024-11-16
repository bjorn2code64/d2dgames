// GXml.h: interface for the Xml class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "wrap32lib.h"
#include "GAlloc.h"

#include <stdio.h>

#include <sstream>

using namespace GAlloc;

#define XML_LOAD_NOSHARE	-4
#define XML_LOAD_EMPTY		-3
#define XML_LOAD_FILEERROR	-2
#define XML_LOAD_NOFILE	-1
#define XML_LOAD_OK		0

#define CRLFSTRINGA	"\r\n"

namespace XML {

	class XmlFile
	{
	public:
		XmlFile()
		{
			m_fp = NULL;
			m_szScan = NULL;
			m_pFileData = NULL;
			m_bAtLT = false;
			m_bAtSingle = false;
			m_bAtAttribute = false;
			m_bAtAttributeValue = false;
			m_pNext = NULL;
			m_szCurrentTag = NULL;
			m_szPath = NULL;
			m_bDataIsAlloc = FALSE;
		}

		~XmlFile();

		// Read support
		bool Load(const char* szName);
		void Free()
		{
			if (m_bDataIsAlloc && m_pFileData)
				free((void*)m_pFileData);

			m_pFileData = NULL;
		}

		void Rewind() { m_szScan = m_pFileData; }
		void Set(char* szData, BOOL bCopy)
		{
			Free();
			m_pFileData = bCopy ? _strdup(szData) : szData;
			m_bDataIsAlloc = bCopy;
			m_szScan = m_pFileData;
			m_nLineNumber = 1;
		}

		enum elementtype
		{
			eteof,
			etstart,
			etend,
			etvalue,
			etfail,
			etattrib,
			etattribvalue
		};

		const char* GetNextToken(elementtype* pet);
		int GetLineNumber() { return m_nLineNumber; }

		const char* GetPath() { return m_szPath; }

		// Write support
		bool OpenForWrite(const char* szName);
		void WriteElementStartOpen(const char* szName, int nDepth);
		void WriteElementStartClose();
		void WriteElementEnd(const char* szName, int nDepth);
		void WriteElementWithValue(const char* szName, const char* szValue, int nDepth);
		void WriteAttribute(const char* szName, const char* szValue);

		// Included file support
		XmlFile* CreateIncludedXMLFile();

	protected:
		void WriteDepth(int nDepth);
		const char* ParseValue(char chTerm);

	protected:
		char* m_pFileData;
		char* m_szScan;
		int m_nFileSize;
		FILE* m_fp;
		int m_nLineNumber;
		bool m_bAtLT;
		bool m_bAtSingle;
		bool m_bAtAttribute;
		bool m_bAtAttributeValue;
		char* m_szCurrentTag;
		XmlFile* m_pNext;	// included files
		char* m_szPath;
		BOOL m_bDataIsAlloc;
	};

#define GXMLS_STATE_NONE		0
#define GXMLS_STATE_OPEN		1
#define GXMLS_STATE_OPENED		2
#define GXMLS_STATE_CLOSED		3

	class XmlSaver
	{
	protected:
		class CElementName
		{
		public:
			CElementName(const char* sz) { m_szName = _strdup(sz); }
			~CElementName() { free(m_szName); }

			char* m_szName;
			CElementName* m_pNext;
		};

	public:
		XmlSaver(FILE* fp, BOOL bCompressed = FALSE);
		XmlSaver(CGAlloc* pAlloc, BOOL bCompressed = FALSE);
		~XmlSaver();

		void Open(const char* szName)
		{
			OpenStart(szName);
			OpenFinish();
		}

		static void NewLineIndent(FILE* fp, CGAlloc* pAlloc, int nDepth)
		{
			if (fp) { fputc('\n', fp); Indent(fp, nDepth); }
			if (pAlloc) { pAlloc->ContinuousAlloc(CRLFSTRINGA);	Indent(pAlloc, nDepth); }
		}

		void NewLineIndent()
		{
			if (m_bCompressed)
				return;

			NewLineIndent(m_fp, m_pAlloc, m_nDepth);
		}

		void OpenStart(const char* szName);
		void OpenFinish(BOOL bEmptyTag = FALSE);
		BOOL Close(const char* szName);
		void Save(const char* szName, const char* szValue = NULL)
		{
			SaveValue(m_fp, m_pAlloc, &m_nState, m_nDepth, szName, szValue, m_bCompressed);
		}

		void Save(const char* szName, int nValue)
		{
			SaveValue(m_fp, m_pAlloc, &m_nState, m_nDepth, szName, nValue, m_bCompressed);
		}

		void Save(const char* szName, DWORD dwValue)
		{
			SaveValue(m_fp, m_pAlloc, &m_nState, m_nDepth, szName, dwValue, m_bCompressed);
		}

		void Save(const char* szName, double dValue)
		{
			SaveValue(m_fp, m_pAlloc, &m_nState, m_nDepth, szName, dValue, m_bCompressed);
		}

		void SaveValue(const char* sz) { SaveEncoded(m_fp, m_pAlloc, sz); }
		void SaveValue(int n) { char buff[256]; _snprintf_s(buff, 256, "%d", n);		SaveEncoded(m_fp, m_pAlloc, buff); }
		void SaveValue(DWORD dw) { char buff[256]; _snprintf_s(buff, 256, "%lu", dw);		SaveEncoded(m_fp, m_pAlloc, buff); }
		void SaveValue(double d) { char buff[256]; _snprintf_s(buff, 256, "%0.15g", d);	SaveEncoded(m_fp, m_pAlloc, buff); }

		void SaveAttribute(const char* szName, const char* szValue) { SaveAttribute(m_fp, m_pAlloc, szName, szValue); }
		void SaveAttribute(const char* szName, int nValue) { SaveAttribute(m_fp, m_pAlloc, szName, nValue); }
		void SaveAttribute(const char* szName, DWORD dwValue) { SaveAttribute(m_fp, m_pAlloc, szName, dwValue); }

		static void SaveValue(FILE* fp, CGAlloc* pAlloc, int* pState, int nDepth, const char* szName, const char* szValue, BOOL bCompressed);
		static void SaveValue(FILE* fp, CGAlloc* pAlloc, int* pState, int nDepth, const char* szName, int nValue, BOOL bCompressed);
		static void SaveValue(FILE* fp, CGAlloc* pAlloc, int* pState, int nDepth, const char* szName, DWORD dwValue, BOOL bCompressed);
		static void SaveValue(FILE* fp, CGAlloc* pAlloc, int* pState, int nDepth, const char* szName, double dValue, BOOL bCompressed);

		static void SaveAttribute(FILE* fp, CGAlloc* pAlloc, const char* szName, const char* szValue);
		static void SaveAttribute(FILE* fp, CGAlloc* pAlloc, const char* szName, int nValue);
		static void SaveAttribute(FILE* fp, CGAlloc* pAlloc, const char* szName, DWORD dwValue);

		static void SaveEncoded(FILE* fp, CGAlloc* pAlloc, const char* szText);

	protected:
		static void Indent(FILE* fp, int nDepth) { for (int i = 0; i < nDepth; i++)	fputc(' ', fp); }
		static void Indent(CGAlloc* pAlloc, int nDepth) { for (int i = 0; i < nDepth; i++)	pAlloc->ContinuousAlloc(" "); }

	protected:
		FILE* m_fp;
		CGAlloc* m_pAlloc;
		CElementName* m_pStack;
		int m_nDepth;
		BOOL m_bCompressed;
		int m_nState;
	};

	class Xml;

#define XML_EL_CHILD_ATTRIBUTE	((XmlElement*)-1)	// Invalid child pointer - indicates attrib

	class XmlElement
	{
	public:
		XmlElement(const char* szName);
		XmlElement(CGAlloc& ga, const char* szName);

		XmlElement(const char* szName, const char* szValue);
		XmlElement(CGAlloc& ga, const char* szName, const char* szValue);

		~XmlElement();

		bool Load(XmlFile& xmlFile, std::stringstream& ssError, Xml* pXML);
		bool LoadTags(XmlFile& xmlFile, std::stringstream& ssError, Xml* pXML);

		const void GetStrippedValue(char* szBuff, int nBuffLen);
		bool GetValueBool();
		int GetValueInt(int nDefault) { const char* szValue = GetValue();	return *szValue ? atoi(szValue) : nDefault; }
		DWORD GetValueDWORD(DWORD dwDefault);
		double GetValueDouble(double dDefault) { const char* szValue = GetValue();	return *szValue ? atof(szValue) : dDefault; }
		void SetValue(CGAlloc& ga, const char* szValue);
		void SetValue(CGAlloc& ga, int nValue);
		void SetValue(CGAlloc& ga, double dValue);

		const char* GetName() { if (!m_szName)	return "";			return m_szName; }
		const char* GetValue() { if (!m_szValue)	return "";			return m_szValue; }
		XmlElement* GetChild() { if (IsAttribute())	return NULL;	return m_pChild; }
		XmlElement* GetNext() { return m_pNext; }

		void SetStaticName(const char* sz) { m_szName = sz; }
		void SetStaticValue(const char* sz) { m_szValue = sz; }
		void SetNext(XmlElement* pEl) { m_pNext = pEl; }
		void SetChild(XmlElement* pEl) { m_pChild = pEl; }

		int GetChildCount() { int n = 0;	for (XmlElement* p = GetChild(); p; p = p->GetNext())	n++;	return n; }

		XmlElement* GetChildByName(const char* szName);
		XmlElement* GetChildByPath(const char* szPath);

		XmlElement* CreateChildByName(CGAlloc& ga, const char* szName);
		XmlElement* CreateChildByPath(CGAlloc& ga, const char* szPath);

		XmlElement* EnsureExists(CGAlloc& ga, const char* szName)
		{
			XmlElement* p = GetChildByName(szName);
			return p ? p : CreateChildByName(ga, szName);
		}

		const char* GetChildValue(const char* szName);
		const char* GetChildValueString(const char* szName, const char* szDefault);
		int GetChildValueInt(const char* szName, int nDefault);

		void Dump(int nDepth = 0);

		bool Save(XmlFile& xmlFile, int nDepth);
		void Save(XmlSaver& xmls);
		XmlElement* AddChild(XmlElement* pE);
		XmlElement* AddChild(const char* szName, const char* szValue, BOOL bAttribute = FALSE);
		XmlElement* AddChild(CGAlloc& ga, const char* szName, const char* szValue, BOOL bAttribute = FALSE);
		XmlElement* AddChild(CGAlloc& ga, const char* szName, int nValue, BOOL bAttribute = FALSE) { char buff[256]; _snprintf_s(buff, 256, "%d", nValue);	return AddChild(ga, szName, buff, bAttribute); }
		void DeleteAllChildren() { if (m_pChild && !IsAttribute()) { delete m_pChild; m_pChild = NULL; } }
		void DeleteChild(XmlElement* pChild)
		{
			if (m_pChild == pChild)
				m_pChild = pChild->GetNext();
			else
			{
				for (XmlElement* p = m_pChild; p->GetNext(); p = p->GetNext())
				{
					if (p->GetNext() == pChild)
					{
						p->SetNext(pChild->GetNext());
						break;
					}
				}
			}

			pChild->SetNext(NULL);
			delete pChild;
		}


		bool ElementTreeIsIdentical(XmlElement* pEl);
		bool ValueIsInteger();

		BOOL IsAttribute() { return m_pChild == XML_EL_CHILD_ATTRIBUTE; }
		BOOL SetAttribute() { if (m_pChild)	return FALSE;	m_pChild = XML_EL_CHILD_ATTRIBUTE;	return TRUE; }
		void ElementsToAttributes();

	protected:
		const char* m_szName;
		const char* m_szValue;

		XmlElement* m_pNext;
		XmlElement* m_pChild;	// Will be XML_EL_CHILD_ATTRIBUTE if this is an attribute
	};

#define XML_OST_OK					0
#define XML_OST_ALREADY_PROCESSED	1
#define XML_OST_ERROR				2

	class Xml
	{
	public:
		Xml();
		Xml(CGAlloc& ga, const char* szName);
		virtual ~Xml();

		void DeleteAll();

		// Building
		int Load(const char* szName, std::stringstream& ssError, const char* szMainTag);

		BOOL CreateShare(int nMemID);
		BOOL AttachToShare(int nMemID);

		int Set(const char* szName, char* szContents, std::stringstream& ssError, const char* szMainTag, BOOL bCopy = TRUE);
		void Save(XmlSaver& xmls);
		bool Save(const char* szName);

		bool CopyFrom(CGAlloc& ga, XmlElement* pEl);
		bool CopyFrom(CGAlloc& ga, const Xml& xml) { return CopyFrom(ga, xml.GetRootElement()); }

		// Querying
		XmlElement* GetRootElement() const { return m_pRootElement; }
		XmlElement* GetChild() { return m_pRootElement ? m_pRootElement->GetChild() : NULL; }
		XmlElement* GetChildByName(const char* sz) { return m_pRootElement ? m_pRootElement->GetChildByName(sz) : NULL; }
		XmlElement* CreateChildByName(CGAlloc& ga, const char* szName) { return m_pRootElement ? m_pRootElement->CreateChildByName(ga, szName) : NULL; }
		XmlElement* EnsureExists(CGAlloc& ga, const char* szName) { return m_pRootElement ? m_pRootElement->EnsureExists(ga, szName) : NULL; }

		void SetElementByPath(CGAlloc& ga, const char* szPath, const char* szValue);
		XmlElement* GetElementByPath(const char* szPath);

		void Hook(XmlElement* p) { m_pRootElement = p; }

		virtual int OnStartTag(const char* /*szTag*/, XmlFile& /*xmlFile*/, XmlElement* /*pXLParent*/, std::stringstream&) { return XML_OST_OK; }

		void FreeFile() { m_xmlFile.Free(); }

		int GetMemUsage(int* pElements);
		int GetMemUsage(XmlElement* pEl, int* pElements);

		void ElementsToAttributes() { if (m_pRootElement) m_pRootElement->ElementsToAttributes(); }

	protected:
		XmlElement* CopyElementTree(CGAlloc& ga, XmlElement* pEl, bool bCopyNext);
		int Parse(const char* szName, std::stringstream& ssError, const char* szMainTag);

	protected:
		char* m_szFileName;
		XmlFile m_xmlFile;
		BOOL m_bLocalAlloc;

	private:
		XmlElement* m_pRootElement;
	};
}