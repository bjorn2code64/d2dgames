// Xml.cpp: implementation of the Xml class.
//
//////////////////////////////////////////////////////////////////////

#include "Xml.h"

//////////////////////////////////////////////////////////////////////
// Xml - A tree of elements
//////////////////////////////////////////////////////////////////////

namespace XML {
Xml::Xml()
{
	m_pRootElement = NULL;
	m_szFileName = NULL;
	m_bLocalAlloc = FALSE;
}

Xml::Xml(CGAlloc& ga, const char* szName)
{
	m_pRootElement = new XmlElement(ga, szName);
	m_szFileName = NULL;
	m_bLocalAlloc = FALSE;
}

Xml::~Xml()
{
	DeleteAll();
}

void Xml::DeleteAll()
{
	if (m_bLocalAlloc)
	{
		if (m_pRootElement)
		{
			delete m_pRootElement;
			m_pRootElement = NULL;
		}
	}

	if (m_szFileName)
	{
		free(m_szFileName);
		m_szFileName = NULL;
	}
}

//////////////////////////////////////////////////////////////////////

int Xml::Set(const char* szName, char* szContents, std::stringstream& ssError, const char* szMainTag, BOOL bCopy)
{
	if (m_szFileName)
		free(m_szFileName);
	m_szFileName = NULL;

	if (!szContents)
		return XML_LOAD_EMPTY;

	m_bLocalAlloc = TRUE;
	m_xmlFile.Set(szContents, bCopy);
	return Parse(szName, ssError, szMainTag);
}

int Xml::Load(const char* szFileName, std::stringstream& ssError, const char* szMainTag)
{
	m_bLocalAlloc = TRUE;

	if (m_szFileName)
		free(m_szFileName);
	m_szFileName = _strdup(szFileName);
	if (!m_xmlFile.Load(szFileName))
	{
		ssError << szFileName << " : Can't open file";
		m_pRootElement = new XmlElement(szMainTag);
		return XML_LOAD_NOFILE;
	}

	int nRet = Parse(szFileName, ssError, szMainTag);
	if (nRet == XML_LOAD_OK)
		return nRet;

	// Parse error - put the tree back to something we're expecting at least
	DeleteAll();
	m_pRootElement = new XmlElement(szMainTag);
	return nRet;
}

int Xml::Parse(const char* szName, std::stringstream& ssError, const char* szMainTag)
{
	XmlFile::elementtype et;
	const char* szElement = m_xmlFile.GetNextToken(&et);
	if (et == XmlFile::eteof)
	{
		m_pRootElement = new XmlElement(szMainTag ? szMainTag : "MAIN");
		return XML_LOAD_EMPTY;
	}

	if (et != XmlFile::etstart)
	{
		ssError << szName << " : Malformed file (not XML)";
		return XML_LOAD_FILEERROR;
	}

	if (m_pRootElement)
	{
		delete m_pRootElement;
		m_pRootElement = NULL;
	}

	if (szMainTag && strcmp(szElement, szMainTag))
	{
		ssError << szName << " : Expecting " << szMainTag << " got " << szElement;
		return XML_LOAD_FILEERROR;
	}

	m_pRootElement = new XmlElement(szElement);
	if (!m_pRootElement->Load(m_xmlFile, ssError, this))
		return XML_LOAD_FILEERROR;

	XmlElement* pXLScan = m_pRootElement;
	for (;;)
	{
		const char* szElementScan = m_xmlFile.GetNextToken(&et);
		if (et == XmlFile::eteof)
			break;

		if (et != XmlFile::etstart)
		{
			ssError << szName << " : Malformed file (el:" << szElementScan << ")";
			return XML_LOAD_FILEERROR;
		}

		XmlElement* pXLNew = new XmlElement(szElementScan);
		pXLScan->SetNext(pXLNew);
		if (!pXLNew->Load(m_xmlFile, ssError, this))
			return XML_LOAD_FILEERROR;

		pXLScan = pXLNew;
	}

	return XML_LOAD_OK;
}

bool Xml::Save(const char* szName)
{
	const char* szNameToUse = szName;
	if (!szNameToUse)
		szNameToUse = m_szFileName;
	if (!szNameToUse)
		return false;

	XmlFile xmlFile;

	if (!xmlFile.OpenForWrite(szNameToUse))
		return false;

	if (m_pRootElement)
		return m_pRootElement->Save(xmlFile, 0);

	return true;
}

void Xml::Save(XmlSaver& xmls)
{
	if (m_pRootElement)
		m_pRootElement->Save(xmls);
}

void Xml::SetElementByPath(CGAlloc& ga, const char* szPath, const char* szValue)
{
	if (!m_pRootElement)
		return;

	XmlElement* p = m_pRootElement->CreateChildByPath(ga, szPath);
	p->SetValue(ga, szValue);
}

XmlElement* Xml::GetElementByPath(const char* szPath)
{
	if (!m_pRootElement)
		return NULL;

	return m_pRootElement->GetChildByPath(szPath);
}

XmlElement* Xml::CopyElementTree(CGAlloc& ga, XmlElement* pEl, bool bCopyNext)
{
	XmlElement* pElNew = new XmlElement(ga, pEl->GetName());

	if (pEl->GetChild())
		pElNew->AddChild(CopyElementTree(ga, pEl->GetChild(), true));

	if (bCopyNext && pEl->GetNext())
		pElNew->SetNext(CopyElementTree(ga, pEl->GetNext(), true));

	if (*pEl->GetValue())
		pElNew->SetValue(ga, pEl->GetValue());

	return pElNew;
}

bool Xml::CopyFrom(CGAlloc& ga, XmlElement *pEl)
{
	if (m_pRootElement)
	{
		delete m_pRootElement;
		m_pRootElement = NULL;
	}

	if (m_szFileName)
	{
		free(m_szFileName);
		m_szFileName = NULL;
	}

	if (!pEl)
		return false;

	m_pRootElement = CopyElementTree(ga, pEl, false);
	return true;
}

int Xml::GetMemUsage(XmlElement* pEl, int* pElements)
{
	int nRet = sizeof(XmlElement);
	(*pElements)++;

	if (pEl->GetChild())
		nRet += GetMemUsage(pEl->GetChild(), pElements);

	if (pEl->GetNext())
		nRet += GetMemUsage(pEl->GetNext(), pElements);

	return nRet;
}

int Xml::GetMemUsage(int* pElements)
{
	int nUsage = sizeof(Xml);
	if (m_szFileName)
		nUsage += (int)strlen(m_szFileName) + 1;

	if (m_pRootElement)
		nUsage += GetMemUsage(m_pRootElement, pElements);

	return nUsage;
}


//////////////////////////////////////////////////////////////////////
// XmlElement
//////////////////////////////////////////////////////////////////////

XmlSaver::XmlSaver(FILE* fp, BOOL bCompressed)
{
	m_fp = fp;
	m_pAlloc = NULL;
	m_pStack = NULL;
	m_nDepth = 0;
	m_bCompressed = bCompressed;
	m_nState = GXMLS_STATE_NONE;
}

XmlSaver::XmlSaver(CGAlloc* pAlloc, BOOL bCompressed)
{
	m_fp = NULL;
	m_pAlloc = pAlloc;
	m_pStack = NULL;
	m_nDepth = 0;
	m_bCompressed = bCompressed;
	m_nState = GXMLS_STATE_NONE;
}

XmlSaver::~XmlSaver()
{
	while (m_pStack)
	{
		if (!Close(m_pStack->m_szName))
			break;
	}
}

void XmlSaver::OpenStart(const char* szName)
{
	if ((m_nState == GXMLS_STATE_OPENED) || (m_nState == GXMLS_STATE_CLOSED))
		NewLineIndent();

	char buff[256];
	sprintf_s(buff, 256, "<%s", szName);
	if (m_fp)		fputs(buff, m_fp);
	if (m_pAlloc)	m_pAlloc->ContinuousAlloc(buff);

	CElementName* p = new CElementName(szName);
	p->m_pNext = m_pStack;
	m_pStack = p;

	m_nState = GXMLS_STATE_OPEN;
}

void XmlSaver::OpenFinish(BOOL bEmptyTag)
{
	if (!m_pStack)
		return;

	const char* sz = bEmptyTag ? "/>" : ">";

	if (m_fp)		fputs(sz, m_fp);
	if (m_pAlloc)	m_pAlloc->ContinuousAlloc(sz);

	if (bEmptyTag)
	{
		CElementName* p = m_pStack;
		m_pStack = m_pStack->m_pNext;
		delete p;
		m_nState = GXMLS_STATE_CLOSED;
	}
	else
	{
		m_nState = GXMLS_STATE_OPENED;
		m_nDepth++;
	}
}

BOOL XmlSaver::Close(const char* szName)
{
	if (!m_pStack)
		return FALSE;

	m_nDepth--;

	if (m_nState == GXMLS_STATE_CLOSED)
		NewLineIndent();

	if (strcmp(szName, m_pStack->m_szName))
		return FALSE;

	char buff[256];
	sprintf_s(buff, 256, "</%s>", m_pStack->m_szName);
	if (m_fp)		fputs(buff, m_fp);
	if (m_pAlloc)	m_pAlloc->ContinuousAlloc(buff);

	CElementName* p = m_pStack;
	m_pStack = m_pStack->m_pNext;
	delete p;

	m_nState = GXMLS_STATE_CLOSED;
	return TRUE;
}

void XmlSaver::SaveValue(FILE* fp, CGAlloc* pAlloc, int* pState, int nDepth, const char* szName, int nValue, BOOL bCompressed)
{
	char buff[256];
	sprintf_s(buff, 256, "%d", nValue);
	SaveValue(fp, pAlloc, pState, nDepth, szName, buff, bCompressed);
}

void XmlSaver::SaveValue(FILE* fp, CGAlloc* pAlloc, int* pState, int nDepth, const char* szName, DWORD dwValue, BOOL bCompressed)
{
	char buff[256];
	sprintf_s(buff, 256, "%lu", dwValue);
	SaveValue(fp, pAlloc, pState, nDepth, szName, buff, bCompressed);
}

void XmlSaver::SaveValue(FILE* fp, CGAlloc* pAlloc, int* pState, int nDepth, const char* szName, double dValue, BOOL bCompressed)
{
	char buff[256];
	sprintf_s(buff, 256, "%0.15g", dValue);
	SaveValue(fp, pAlloc, pState, nDepth, szName, buff, bCompressed);
}

void XmlSaver::SaveValue(FILE* fp, CGAlloc* pAlloc, int* pState, int nDepth, const char* szName, const char* szValue, BOOL bCompressed)
{
	char buff[256];

	if (!bCompressed &&
		((*pState == GXMLS_STATE_OPENED) || (*pState == GXMLS_STATE_CLOSED))
		)
		NewLineIndent(fp, pAlloc, nDepth);

	sprintf_s(buff, 256, "<%s", szName);
	if (fp)	fputs(buff, fp);
	if (pAlloc) pAlloc->ContinuousAlloc(buff);

	// check for empty (or NULL) value string - use <TAG/> if so
	if (!szValue || !*szValue)
	{
		if (fp)	fputs("/>", fp);
		if (pAlloc) pAlloc->ContinuousAlloc("/>");
	}
	else
	{
		if (fp)	fputs(">", fp);
		if (pAlloc) pAlloc->ContinuousAlloc(">");

		SaveEncoded(fp, pAlloc, szValue);

		sprintf_s(buff, 256, "</%s>", szName);
		if (fp)	fputs(buff, fp);
		if (pAlloc) pAlloc->ContinuousAlloc(buff);
	}

	*pState = GXMLS_STATE_CLOSED;
}

void XmlSaver::SaveAttribute(FILE* fp, CGAlloc* pAlloc, const char* szName, int nValue)
{
	char buff[256];
	sprintf_s(buff, 256, "%d", nValue);
	SaveAttribute(fp, pAlloc, szName, buff);
}

void XmlSaver::SaveAttribute(FILE* fp, CGAlloc* pAlloc, const char* szName, DWORD dwValue)
{
	char buff[256];
	sprintf_s(buff, 256, "%lu", dwValue);
	SaveAttribute(fp, pAlloc, szName, buff);
}

void XmlSaver::SaveAttribute(FILE* fp, CGAlloc* pAlloc, const char* szName, const char* szValue)
{
	if (fp)	fputs(" ", fp);
	if (pAlloc) pAlloc->ContinuousAlloc(" ");

	if (fp)	fputs(szName, fp);
	if (pAlloc) pAlloc->ContinuousAlloc(szName);

	if (fp)	fputs("=\"", fp);
	if (pAlloc) pAlloc->ContinuousAlloc("=\"");

	SaveEncoded(fp, pAlloc, szValue);

	if (fp)	fputs("\"", fp);
	if (pAlloc) pAlloc->ContinuousAlloc("\"");
}

void XmlSaver::SaveEncoded(FILE* fp, CGAlloc* pAlloc, const char* szText)
{
	if (!szText)
		return;

	char buff[256];
	const char* szOut = NULL;
	buff[1] = '\0';
	for (const char* szScan = szText; *szScan; szScan++)
	{
		if (*szScan == '<')
			szOut = "&lt;";
		else if (*szScan == '>')
			szOut = "&gt;";
		else if (*szScan == '&')
			szOut = "&amp;";
		else if (*szScan == '\'')
			szOut = "&apos;";
		else if (*szScan == '"')
			szOut = "&quot;";
		else
		{
			buff[0] = *szScan;
			szOut = buff;
		}

		if (fp)	fputs(szOut, fp);
		if (pAlloc) pAlloc->ContinuousAlloc(szOut);
	}
}

//////////////////////////////////////////////////////////////////////
// XmlElement
//////////////////////////////////////////////////////////////////////

XmlElement::XmlElement(const char* szName)
{
	SetStaticName(szName);
	m_szValue = NULL;
	m_pNext = NULL;
	m_pChild = NULL; 
}

XmlElement::XmlElement(const char* szName, const char* szValue)
{
	SetStaticName(szName); 
	SetStaticValue(szValue);
	m_pNext = NULL;
	m_pChild = NULL; 
}

XmlElement::XmlElement(CGAlloc& ga, const char* szName, const char* szValue)
{
	SetStaticName(ga.Alloc(szName));
	SetStaticValue(ga.Alloc(szValue));
	m_pNext = NULL;
	m_pChild = NULL;
}

XmlElement::XmlElement(CGAlloc& ga, const char* szName)
{
	SetStaticName(ga.Alloc(szName));
	m_szValue = NULL;
	m_pNext = NULL;
	m_pChild = NULL;
}

XmlElement::~XmlElement()
{
	if (GetChild())
		delete GetChild();

	if (GetNext())
		delete GetNext();
}

XmlElement* XmlElement::AddChild(XmlElement* pE)
{
	XmlElement* pElChild = GetChild();
	if (!pElChild)
		SetChild(pE);
	else
	{
		if (!pElChild->GetNext())
			pElChild->SetNext(pE);
		else
		{
			XmlElement* pScan = pElChild->GetNext();
			for (; pScan->GetNext(); pScan = pScan->GetNext())
				;
			pScan->SetNext(pE);
		}

		pE->SetNext(NULL);
	}

	return pE;
}

XmlElement* XmlElement::AddChild(const char* szName, const char* szValue, BOOL bAttribute)
{
	XmlElement* pEl = new XmlElement(szName, szValue);
	if (bAttribute)
		pEl->SetAttribute();
	return AddChild(pEl);
}

XmlElement* XmlElement::AddChild(CGAlloc& ga, const char* szName, const char* szValue, BOOL bAttribute)
{
	XmlElement* pEl = new XmlElement(ga, szName, szValue);
	if (bAttribute)
		pEl->SetAttribute();
	return AddChild(pEl);
}

bool XmlElement::LoadTags(XmlFile& xmlFile, std::stringstream& ssError, Xml* pXML)
{
	XmlFile::elementtype et;
	for (const char* szNewElement = xmlFile.GetNextToken(&et); szNewElement; szNewElement = xmlFile.GetNextToken(&et))
	{
		if ((et == XmlFile::etend) && !strcmp(GetName(), szNewElement))
			return true;

		switch (pXML->OnStartTag(szNewElement, xmlFile, this, ssError))
		{
		case XML_OST_ERROR:
			return false;
	
		case XML_OST_OK:
			{
				XmlElement* pElement = new XmlElement(szNewElement);
				if (!pElement->Load(xmlFile, ssError, pXML))
				{
					ssError << "Error (Line " << xmlFile.GetLineNumber() << "): Can't load element " << pElement->GetName();
					return false;
				}
				AddChild(pElement);
			}
			break;

		case XML_OST_ALREADY_PROCESSED:
			break;
		}
	}

	return true;
}

bool XmlElement::Load(XmlFile& xmlFile, std::stringstream& ssError, Xml* pXML)
{
	XmlFile::elementtype et;
	const char* szElement = xmlFile.GetNextToken(&et);

	if (!szElement)
		return false;

	while ((et == XmlFile::etstart) || (et == XmlFile::etvalue) || (et == XmlFile::etattrib) || (et == XmlFile::etattribvalue))
	{
		if (et == XmlFile::etstart)
		{
			switch (pXML->OnStartTag(szElement, xmlFile, this, ssError))
			{
			case XML_OST_ERROR:
				return false;

			case XML_OST_OK:
				{
					XmlElement* pElement = new XmlElement(szElement);
					AddChild(pElement);
					if (!pElement->Load(xmlFile, ssError, pXML))
					{
						ssError << "Error (Line " << xmlFile.GetLineNumber() << "): Can't load element " << pElement->GetName();
						return false;
					}
				}
				break;

			case XML_OST_ALREADY_PROCESSED:
				break;
			}
		}
		else if (et == XmlFile::etattrib)
		{
			switch (pXML->OnStartTag(szElement, xmlFile, this, ssError))
			{
			case XML_OST_ERROR:
				return false;

			case XML_OST_OK:
				{
					XmlElement* pElement = new XmlElement(szElement);
					pElement->SetAttribute();
					AddChild(pElement);
					if (!pElement->Load(xmlFile, ssError, pXML))
					{
						ssError << "Error (Line " << xmlFile.GetLineNumber()  << "): Can't load element " << pElement->GetName();
						return false;
					}
				}
				break;

			case XML_OST_ALREADY_PROCESSED:
				break;
			}
		}
		else if (et == XmlFile::etvalue)
		{
			SetStaticValue(szElement);
		}
		else if (et == XmlFile::etattribvalue)
		{
			SetStaticValue(szElement);
			return true;
		}

		szElement = xmlFile.GetNextToken(&et);
	}

	// End this element
	if ((et == XmlFile::etend) && !strcmp(GetName(), szElement))
		return true;

	ssError << "Error (Line " << xmlFile.GetLineNumber()  << "): No match for element " << GetName();
	return false;	// etfail or eteof
}

bool XmlElement::Save(XmlFile& xmlFile, int nDepth)
{
	if (GetChild())
	{
		xmlFile.WriteElementStartOpen(GetName(), nDepth);
		XmlElement* pScan;
		for (pScan = GetChild(); pScan; pScan = pScan->GetNext())
		{
			if (pScan->IsAttribute())
				xmlFile.WriteAttribute(pScan->GetName(), pScan->GetValue());
		}

		xmlFile.WriteElementStartClose();

		for (pScan = GetChild(); pScan; pScan = pScan->GetNext())
		{
			if (!pScan->IsAttribute())
				pScan->Save(xmlFile, nDepth + 1);
		}

		xmlFile.WriteElementEnd(GetName(), nDepth);
	}
	else
		xmlFile.WriteElementWithValue(GetName(), GetValue(), nDepth);

	return true;
}

void XmlElement::Save(XmlSaver& xmls)
{
	// Do we have children and/or attributes?
	BOOL bHasChildren = FALSE;
	BOOL bHasAttribs = FALSE;
	const char* szValue = GetValue();
	BOOL bHasValue = szValue && *szValue;

	for (XmlElement* pEl = GetChild(); pEl; pEl = pEl->GetNext())
	{
		if (!pEl->IsAttribute())
			bHasChildren = TRUE;
		else
			bHasAttribs = TRUE;
	}

	// Save the atributes if there are any
	if (bHasAttribs)
	{
		xmls.OpenStart(GetName());
		for (XmlElement* pEl = GetChild(); pEl; pEl = pEl->GetNext())
		{
			if (pEl->IsAttribute())
				xmls.SaveAttribute(pEl->GetName(), pEl->GetValue());
		}

		if (!bHasValue && !bHasChildren)
		{
			xmls.OpenFinish(TRUE);
			return;
		}

		xmls.OpenFinish();
	}
	else
		xmls.Open(GetName());

	if (bHasChildren)
	{
		for (XmlElement* pEl = GetChild(); pEl; pEl = pEl->GetNext())
		{
			if (!pEl->IsAttribute())
				pEl->Save(xmls);
		}
	}

	if (bHasValue)
		xmls.SaveValue(szValue);

	xmls.Close(GetName());
}

void XmlElement::SetValue(CGAlloc& ga, const char* szValue)
{
	SetStaticValue(ga.Alloc(szValue));
}

void XmlElement::SetValue(CGAlloc& ga, int nValue)
{
	char buff[256];
	sprintf_s(buff, 256, "%d", nValue);
	SetStaticValue(ga.Alloc(buff));
}

void XmlElement::SetValue(CGAlloc& ga, double dValue)
{
	char buff[256];
	sprintf_s(buff, 256, "%.15g", dValue);
	SetStaticValue(ga.Alloc(buff));
}

DWORD XmlElement::GetValueDWORD(DWORD dwDefault)
{
	DWORD dw;
	if (sscanf_s(GetValue(), "%lu", &dw) == 1)
		return dw;
	return dwDefault;
}

XmlElement* XmlElement::GetChildByName(const char* szName)
{
	for (XmlElement* pScan = GetChild(); pScan; pScan = pScan->GetNext())
	{
		if (!strcmp(pScan->GetName(), szName))
			return pScan;
	}

	return NULL;
}

XmlElement* XmlElement::CreateChildByName(CGAlloc& ga, const char* szName)
{
/*	for (XmlElement* pScan = GetChild(); pScan; pScan = pScan->GetNext())
	{
		if (!strcmp(pScan->GetName(), szName))
			return pScan;
	}*/	// we may need ?1 child of this name - can't just return the first!

	XmlElement* pRet = new XmlElement(ga.Alloc(szName));
	AddChild(pRet);
	return pRet;
}

const char* XmlElement::GetChildValue(const char* szName)
{
	XmlElement* pChild = GetChildByName(szName);
	return pChild ? pChild->GetValue() : NULL;
}

const char* XmlElement::GetChildValueString(const char* szName, const char* szDefault)
{
	XmlElement* pChild = GetChildByName(szName);
	return pChild ? pChild->GetValue() : szDefault;
}

int XmlElement::GetChildValueInt(const char* szName, int nDefault)
{
	XmlElement* pChild = GetChildByName(szName);
	return pChild ? pChild->GetValueInt(nDefault) : nDefault;
}

XmlElement* XmlElement::GetChildByPath(const char* szPath)
{
	const char* szScan = szPath;

	XmlElement* pElementRet = this;

	while (*szScan)
	{
		const char* szElement = szScan;
		while (*szScan && (*szScan != ' '))
			szScan++;

		size_t nLen = szScan - szElement;
		char buff[1024];
		strncpy_s(buff, 1024, szElement, nLen);
		buff[nLen] = '\0';
		pElementRet = pElementRet->GetChildByName(buff);

		if (!pElementRet)
			return NULL;

		while (*szScan && (*szScan == ' '))
			szScan++;
	}

	return pElementRet;
}

XmlElement* XmlElement::CreateChildByPath(CGAlloc& ga, const char* szPath)
{
	const char* szScan = szPath;

	XmlElement* pElementRet = this;

	while (*szScan)
	{
		const char* szElement = szScan;
		while (*szScan && (*szScan != ' '))
			szScan++;

		size_t nLen = szScan - szElement;
		char buff[1024];
		strncpy_s(buff, 1024, szElement, nLen);
		buff[nLen] = '\0';
		XmlElement* pElementNext = pElementRet->GetChildByName(buff);

		if (!pElementNext)
			pElementNext = pElementRet->CreateChildByName(ga, buff);

		pElementRet = pElementNext;

		while (*szScan && (*szScan == ' '))
			szScan++;
	}

	return pElementRet;
}

bool XmlElement::GetValueBool()
{
	const char* szValue = GetValue();
	if (!_stricmp(szValue, "y") || !_stricmp(szValue, "yes") || !_stricmp(szValue, "t") || !_stricmp(szValue, "true"))
		return true;

	if (!_stricmp(szValue, "n") || !_stricmp(szValue, "no") || !_stricmp(szValue, "f") || !_stricmp(szValue, "false"))
		return false;

	if (atoi(szValue) > 0)
		return true;

	return false;
}

bool XmlElement::ElementTreeIsIdentical(XmlElement* pEl)
{
	if (!pEl)
		return false;

	if (strcmp(GetName(), pEl->GetName()))
		return false;	// Name is different

	if (strcmp(GetValue(), pEl->GetValue()))
		return false;

	// Check the children
	if (GetChild())
	{
		if (!GetChild()->ElementTreeIsIdentical(pEl->GetChild()))
			return false;
	}
	else if (pEl->GetChild())
		return false;

	if (GetNext())
	{
		if (!GetNext()->ElementTreeIsIdentical(pEl->GetNext()))
			return false;
	}
	else if (pEl->GetNext())
		return false;

	return true;
}

void XmlElement::Dump(int /*nDepth*/)
{
/*	for (int i = 0; i < nDepth; i++)
		printf(" ");
	printf("%s\n", GetName());
	for (XmlElement* pScan = GetChild(); pScan; pScan = pScan->GetNext())
		pScan->Dump(nDepth + 1);*/
}

bool XmlElement::ValueIsInteger()
{
	if (!GetValue())
		return false;

	for (const char* sz = GetValue(); *sz; sz++)
	{
		if (!isdigit(*sz))
			return false;
	}

	return true;
}

const void XmlElement::GetStrippedValue(char* szBuff, int nBuffLen)
{
	const char* sz = GetValue();
	while (!isalnum(*sz))
		sz++;

	strncpy_s(szBuff, nBuffLen - 1, sz, nBuffLen - 1);
	szBuff[nBuffLen - 1] = '\0';

	int nLen = (int)strlen(szBuff);
	while ((nLen > 0) && !isalnum(szBuff[nLen - 1]))
		nLen--;
	szBuff[nLen] = '\0';
}

void XmlElement::ElementsToAttributes()
{
	if (IsAttribute())
		return;

	if (!GetChild())
	{
		SetAttribute();
		return;
	}

	for (XmlElement* pScan = GetChild(); pScan; pScan = pScan->GetNext())
		pScan->ElementsToAttributes();
}

//////////////////////////////////////////////////////////////////////
// XmlFile - tokenise a file into elements (start, end and value)
//////////////////////////////////////////////////////////////////////

XmlFile::~XmlFile()
{
	if (m_bDataIsAlloc && m_pFileData)
		delete [] m_pFileData;
	
	if (m_fp)
		fclose(m_fp);

	if (m_szPath)
		free(m_szPath);

	if (m_pNext)
		delete m_pNext;
}

bool XmlFile::Load(const char* szName)
{
	FILE* fp;
	if (fopen_s(&fp, szName, "r"))
		return false;

	m_szPath = _strdup(szName);
	char* sz = strrchr(m_szPath, '/');
	if (!sz)
		sz = strrchr(m_szPath, '\\');
	if (sz)
		*sz = '\0';
	else
		m_szPath[0] = '\0';	// no path

	// Stop repeated load attempts from leaking memory
	if (m_bDataIsAlloc && m_pFileData)
		delete [] m_pFileData;

	// try to read encrypted file
	// just assume file is plaintext
	fseek(fp, 0, SEEK_END);
	int nDataSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	m_pFileData = new char[nDataSize + 1];
	if (!m_pFileData)
	{
		fclose(fp);
		return false;
	}

	char* szTemp = (char*)malloc(nDataSize + 1);
	if (!szTemp)
	{
		delete[] m_pFileData;
		fclose(fp);
		return false;
	}

	m_nFileSize = (int)fread(szTemp, 1, nDataSize, fp);
	szTemp[m_nFileSize] = '\0';

	strncpy_s(m_pFileData, nDataSize + 1, szTemp, nDataSize + 1);
	free(szTemp);

	m_bDataIsAlloc = TRUE;

	m_szScan = m_pFileData;
	fclose(fp);

	m_bAtLT = false;
	m_nLineNumber = 1;
	return true;
}

bool XmlFile::OpenForWrite(const char* szName)
{
	if (m_fp)
		fclose(m_fp);

	m_szScan = (char*)"";
	fopen_s(&m_fp, szName, "w");
	m_nLineNumber = 0;
	return m_fp != NULL;
}

const char* XmlFile::ParseValue(char chTerm)
{
	char* szOut = m_szScan;
	while (*m_szScan && (*m_szScan != chTerm))
	{
		if (*m_szScan == '\n')
		{
			m_nLineNumber++;
		}
		else if (*m_szScan == '&')
		{
			m_szScan++;
			if (!strncmp(m_szScan, "amp;", 4))
			{
				*(szOut++) = '&';
				m_szScan += 4;
			}
			else if (!strncmp(m_szScan, "lt;", 3))
			{
				*(szOut++) = '<';
				m_szScan += 3;
			}
			else if (!strncmp(m_szScan, "gt;", 3))
			{
				*(szOut++) = '>';
				m_szScan += 3;
			}
			else if (!strncmp(m_szScan, "apos;", 5))
			{
				*(szOut++) = '\'';
				m_szScan += 5;
			}
			else if (!strncmp(m_szScan, "quot;", 5))
			{
				*(szOut++) = '"';
				m_szScan += 5;
			}
			else if (*m_szScan == '#')
			{
				m_szScan++;
				unsigned int nValue = 0;
				int nCount = 0;
				if (*m_szScan == 'x')				// hex number
				{
					m_szScan++;
					sscanf_s(m_szScan, "%x;%n", &nValue, &nCount);
				}
				else								// decimal number
					sscanf_s(m_szScan, "%u;%n", &nValue, &nCount);

				if (nValue)
				{
					// convert value to UTF8
					if (nValue <= 0x7f)				// 1 byte sequence
						*(szOut++) = (BYTE)nValue;
					else if (nValue <= 0x7ff)		// 2 byte sequence
					{
						*(szOut++) = (BYTE)(0xc0 + (nValue >> 6));
						*(szOut++) = (BYTE)(0x80 + (nValue & 0x3f));
					}
					else if (nValue <= 0xffff)		// 3 byte sequence
					{
						*(szOut++) = (BYTE)(0xe0 + (nValue >> 12));
						*(szOut++) = (BYTE)(0x80 + ((nValue >> 6) & 0x3f));
						*(szOut++) = (BYTE)(0x80 + (nValue & 0x3f));
					}
					else if (nValue <= 0x1fffff)	// 4 byte sequence
					{
						*(szOut++) = (BYTE)(0xf0 + (nValue >> 18));
						*(szOut++) = (BYTE)(0x80 + ((nValue >> 12) & 0x3f));
						*(szOut++) = (BYTE)(0x80 + ((nValue >> 6) & 0x3f));
						*(szOut++) = (BYTE)(0x80 + (nValue & 0x3f));
					}
				}
				m_szScan += nCount;
			}
			continue;
		}

		*(szOut++) = *(m_szScan++);
	}

	if (szOut != m_szScan)
		*szOut = '\0';

	return szOut;
}

// Note that "newline" can be either CRLF or LF, so
// we'll have to account for extra CR characters.
const char* XmlFile::GetNextToken(elementtype* pet)
{
	if (m_bAtSingle)	// just return a blank value
	{
		m_bAtSingle = false;
		if (*m_szScan != '>')
		{
			*pet = etfail;
			return NULL;
		}

		m_szScan++;
		*pet = etend;
		return m_szCurrentTag;
	}

	if (m_bAtAttribute)
	{
		m_bAtAttribute = false;
		char* szElement = m_szScan;

		// Only alpha numeric and _ and : allowed.
		while (*m_szScan && (isalnum(*m_szScan) || (*m_szScan == '_') || (*m_szScan == ':')))
			m_szScan++;

		if (*m_szScan != '=')
		{
			*pet = etfail;
			return NULL;
		}
		*(m_szScan++) = '\0';	// 
		*pet = etattrib;
		m_bAtAttributeValue = true;
		return szElement;
	}

	if (m_bAtAttributeValue)
	{
		m_bAtAttributeValue = false;
		if (*(m_szScan++) != '\"')
		{
			*pet = etfail;
			return NULL;
		}

		const char* szValue = m_szScan;
		ParseValue('\"');
		if (*m_szScan != '\"')
		{
			*pet = etfail;
			return NULL;
		}

		*pet = etattribvalue;
		*(m_szScan)++ = '\0';

		// Now we could do to work out what's next
		// Skip whitespace
		while (*m_szScan && strchr(" \t\n\r", *m_szScan))
		{
			if (*m_szScan == '\n')
				m_nLineNumber++;

			m_szScan++;
		}

		if (*m_szScan == '/')
		{
			m_bAtSingle = true;
			m_szScan++;
		}
		else if (*m_szScan == '>')
			m_szScan++;		
		else
			m_bAtAttribute = true;			// Another attrib ?

		return szValue;
	}

	if (!m_bAtLT)
	{
		// Look ahead for an open tag - only skip white space
		char* szLookAhead = m_szScan;
		while (*szLookAhead && strchr(" \t\n\r", *szLookAhead))
		{
			if (*szLookAhead == '\n')
				m_nLineNumber++;

			szLookAhead++;
		}

		if (*szLookAhead == '<')	// Found an open tag?
		{
			m_szScan = szLookAhead;	// If an open tag, skip to it
		}
		else if (*szLookAhead == '\0')	// End of file?
		{
			m_szScan = szLookAhead;
			*pet = eteof;
			return NULL;
		}
	}

////////////////////////////////////////////////////////////////////////////////
// Work out if we're at etstart, etend or etvalue
////////////////////////////////////////////////////////////////////////////////

	if ((*m_szScan == '<') || m_bAtLT)
	{
		if (!m_bAtLT)
			m_szScan++;
		else
			m_bAtLT = false;

		if (*m_szScan == '/')
		{
			*pet = etend;
			m_szScan++;
		}
		else
			*pet = etstart;
	}
	else
		*pet = etvalue;

////////////////////////////////////////////////////////////////////////////////
// Now read the element name or value in
////////////////////////////////////////////////////////////////////////////////

	char* szElement = m_szScan;

	if (*pet == etvalue)	// Read the value up to a '<'
	{
		// Look for the closing element
		ParseValue('<');
		if (*m_szScan == '<')
		{
			m_bAtLT = true;
			*(m_szScan++) = '\0';
		}
	}
	else
	{
		// only alpha numeric and _ and : allowed
		while (*m_szScan && (isalnum(*m_szScan) || (*m_szScan == '_') || (*m_szScan == ':')))
			m_szScan++;
	}

////////////////////////////////////////////////////////////////////////////////
// Check for valid termination and skip to next token
////////////////////////////////////////////////////////////////////////////////

	if ((*pet == etstart) || (*pet == etend))
	{
		// Skip (and null out to ensure termination of tag) whitespace
		while (*m_szScan && strchr(" \t\n\r", *m_szScan))
		{
			if (*m_szScan == '\n')
				m_nLineNumber++;

			*(m_szScan++) = '\0';
		}

		if ((*pet == etstart) && (*m_szScan == '/'))	// single tag of the form <tag/>
		{
			m_bAtSingle = true;
			*(m_szScan++) = '\0';
		}
		else if (*m_szScan == '>')	// End tag
		{
			*(m_szScan++) = '\0';
		}
		else if (*pet == etstart)
		{
			// assume an attribute
			m_bAtAttribute = true;
		}
		else
		{
			// Bad termination of an etend
			*pet = etfail;
			return NULL;
		}
	}
	else if (*pet == etvalue)
	{
		if (!m_bAtLT)
		{
			*pet = etfail;
			return NULL;
		}
	}

	if (*pet == etstart)
		m_szCurrentTag = szElement;	// save the tag in case we need it for "virtual close"

	return szElement;
}

//////////////////////////////////////////////////////////////////////

void XmlFile::WriteDepth(int nDepth)
{
	while (nDepth > 0)
	{
		fprintf(m_fp, " ");
		nDepth--;
	}
}

void XmlFile::WriteElementStartOpen(const char* szName, int nDepth)
{
	WriteDepth(nDepth);
	fprintf(m_fp, "<%s", szName);
}

void XmlFile::WriteElementStartClose()
{
	fprintf(m_fp, ">\n");
}

void XmlFile::WriteElementEnd(const char* szName, int nDepth)
{
	WriteDepth(nDepth);
	fprintf(m_fp, "</%s>\n", szName);
}

void XmlFile::WriteElementWithValue(const char* szName, const char* szValue, int nDepth)
{
	WriteDepth(nDepth);
	fprintf(m_fp, "<%s>", szName);
	if (szValue)
	{
		for (const char* szScan = szValue; *szScan; szScan++)
		{
			if (*szScan == '<')
				fprintf(m_fp, "&lt;");
			else if (*szScan == '>')
				fprintf(m_fp, "&gt;");
			else if (*szScan == '&')
				fprintf(m_fp, "&amp;");
			else
				fprintf(m_fp, "%c", *szScan);
		}
	}
	fprintf(m_fp, "</%s>", szName);
}

void XmlFile::WriteAttribute(const char* szName, const char* szValue)
{
	fprintf(m_fp, " %s=\"%s\"", szName, szValue);
}

XmlFile* XmlFile::CreateIncludedXMLFile()
{
	XmlFile* pXLF = new XmlFile;
	if (!m_pNext)
		m_pNext = pXLF;
	else
	{
		XmlFile* pScan;
		for (pScan = m_pNext; pScan->m_pNext; pScan = pScan->m_pNext)
			;
		pScan->m_pNext = pXLF;
	}

	return pXLF;
}

//////////////////////////////////////////////////////////////////////
}