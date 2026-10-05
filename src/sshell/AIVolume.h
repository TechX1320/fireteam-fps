//------------------------------------------------------------------------------//
//
// MODULE   : AIVolume.h
//
// PURPOSE  : AIVolume - Definition
//
// CREATED  : 10/23/2002
//
// (c) 2002 LithTech, Inc.  All Rights Reserved
//
//------------------------------------------------------------------------------//

#ifndef __AIVolume_H__
#define __AIVolume_H__


// Engine includes
#include <ltbasedefs.h>
#include <ltengineobjects.h>


//-----------------------------------------------------------------------------
class ZombieSpawner : public BaseClass
{

public:

    ZombieSpawner()
		: m_hTarget(NULL),
		  m_NumSeals(0)
	{
	}

	~ZombieSpawner()
	{
	}

	// EngineMessageFn handlers
	uint32		EngineMessageFn(uint32 messageID, void *pData, float fData);

	void		DecrementSealCount()		{ --m_NumSeals; }

private:

	uint32		PreCreate(void *pData, float fData);
	uint32		TouchNotify(void *pData, float fData);
	void		ReadProps(ObjectCreateStruct* pStruct);
    void        PingTarget();
    void        Spawn();

private:

    HOBJECT     m_hTarget;
    char        m_sTarget[32];
    LTVector    m_vDims;
	int			m_NumSeals;
};


// Imported Jupiter/NOLF2-style AI navigation volume.
// Intentionally inert during the Cabin Fever bring-up milestone.
class AIVolume : public BaseClass
{
public:
    AIVolume();
    ~AIVolume() {}

    uint32 EngineMessageFn(uint32 messageID, void *pData, float fData);

    const LTVector& GetDims() const { return m_vDims; }
    const char* GetRegionName() const { return m_sRegion; }
    const char* GetVolumeName() const { return m_sName; }
    bool IsLit() const { return m_bLit; }
    bool IsPreferredPath() const { return m_bPreferredPath; }
    bool Contains2D(const LTVector& vPos) const;

private:
    void ReadNavigationProps(ObjectCreateStruct *pOCS);

    char     m_sName[64];
    char     m_sRegion[64];
    char     m_sLightSwitchNode[64];
    LTVector m_vDims;
    bool     m_bLit;
    bool     m_bPreferredPath;
};
#endif // __AIVolume_H__
