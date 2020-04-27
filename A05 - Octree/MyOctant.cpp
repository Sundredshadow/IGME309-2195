#include "MyOctant.h"
using namespace Simplex;

uint MyOctant::m_uOctantCount = 2;
uint MyOctant::m_uMaxLevel = 2;
uint MyOctant::m_uIdealEntityCount = 5;

//gets
uint MyOctant::GetOctantCount(void) { return m_uOctantCount; }
float MyOctant::GetSize(void) { return m_fSize; }
vector3 MyOctant::GetMaxGlobal(void) { return m_v3Max; }
vector3 MyOctant::GetMinGlobal(void) { return m_v3Min; }
vector3 MyOctant::GetCenterGlobal(void) { return m_v3Center; }
MyOctant* Simplex::MyOctant::GetChild(uint a_nChild){return m_pChild[a_nChild];}
MyOctant* Simplex::MyOctant::GetParent(void){return m_pParent;}


//initalize
void Simplex::MyOctant::Init(void)
{
	m_uChildren = 0;
	m_fSize = 0;
	m_uLevel = 0;
	m_uID = m_uOctantCount;

	m_v3Center = vector3(0.f);
	m_v3Max = vector3(0.f);
	m_v3Min = vector3(0.f);

	m_pMeshMngr = MeshManager::GetInstance();
	m_pEntityMngr = MyEntityManager::GetInstance();

	m_pRoot = nullptr;
	m_pParent = nullptr;

	for (uint n = 0; n < 8; n++) 
	{
		m_pChild[n] = nullptr;
	}
}

void Simplex::MyOctant::Release(void)
{
	if (m_uLevel==0) 
	{
		KillBranches();
	}
	m_uChildren = 0;
	m_fSize = 0.0f;
	m_EntityList.clear();
	m_lChild.clear();

}


//constructors//////////////////////////////////////////////////////////////////////////////
Simplex::MyOctant::MyOctant(uint a_nMaxLevel, uint a_nIdealEntityCount)
{
	Init();
	m_uOctantCount = 0;//only increments upon instantiation
	m_uMaxLevel = a_nMaxLevel;
	m_uIdealEntityCount = a_nIdealEntityCount;
	m_uID = m_uOctantCount;

	m_pRoot = this;
	m_lChild.clear();


	//Need to get some of the properties we need later on such as max, min, center, and size
	//can get that by creating a rigibody and having it calculate these values
	//but before that the min and max of the rigidbodies of each enitity need to be added to a list

	//A list for max and mins
	std::vector<vector3> lMinMax;
	//then extract the min and maxs
	for (uint a=0; a< m_pEntityMngr->GetEntityCount(); a++)//iterates through all the entities
	{
		//get rigidbodies then get min and max and add to list
		lMinMax.push_back(m_pEntityMngr->GetEntity(a)->GetRigidBody()->GetMinGlobal());
		lMinMax.push_back(m_pEntityMngr->GetEntity(a)->GetRigidBody()->GetMaxGlobal());
	}

	//using the filled list create a rb
	MyRigidBody* tempRb = new MyRigidBody(lMinMax);

	vector3 halfWidth = tempRb->GetHalfWidth();
	//get maximum half width (so it will be a square and not a rectangle)
	//easier later on
	float Max = halfWidth.x;
	if (Max<halfWidth.y) {Max = halfWidth.y;}
	if (Max<halfWidth.z) {Max = halfWidth.z;}

	m_v3Center = tempRb->GetCenterLocal();//get center before deleting rb
	lMinMax.clear();
	SafeDelete(tempRb);//no longer need rb

	//calculate values
	m_fSize = Max*2;
	m_v3Max = m_v3Center+vector3(Max);
	m_v3Min = m_v3Center-vector3(Max);

	m_uOctantCount++;

	ConstructTree(m_uMaxLevel);
}

Simplex::MyOctant::MyOctant(vector3 a_v3Center, float a_fSize)
{
	Init();
	m_v3Center = a_v3Center;
	m_fSize = a_fSize;

	m_v3Max = m_v3Center + vector3(m_fSize)/2.f;
	m_v3Min = m_v3Center - vector3(m_fSize)/ 2.f;

	m_uOctantCount++;
}


Simplex::MyOctant::MyOctant(MyOctant const& other)
{
	m_uID = other.m_uID;
	m_pParent = other.m_pParent;
	m_uChildren = other.m_uChildren;
	m_v3Center = other.m_v3Center;
	m_v3Min = other.m_v3Min;
	m_v3Max = other.m_v3Max;
	m_fSize = other.m_fSize;
	m_uLevel = other.m_uLevel;
	m_pRoot= other.m_pRoot;
	m_lChild= other.m_lChild;

	m_pMeshMngr = MeshManager::GetInstance();
	m_pEntityMngr = MyEntityManager::GetInstance();

	for (uint a=0; a<8 ;a++) 
	{
		m_pChild[a] = other.m_pChild[a];
	}
}

MyOctant& Simplex::MyOctant::operator=(MyOctant const& other)
{
	if (this!=&other) 
	{
		Release();
		Init();
		MyOctant oct(other);
		Swap(oct);
	}
	return *this;
}

void Simplex::MyOctant::Swap(MyOctant& other)
{
	std::swap(m_fSize, other.m_fSize);
	std::swap(m_uID, other.m_uID);
	std::swap(m_v3Center, other.m_v3Center);
	std::swap(m_v3Min, other.m_v3Min);
	std::swap(m_v3Max, other.m_v3Max);
	std::swap(m_uChildren,other.m_uChildren);
	std::swap(m_pRoot,other.m_pRoot);
	std::swap(m_lChild, other.m_lChild);

	
	m_pMeshMngr = MeshManager::GetInstance();
	m_pEntityMngr = MyEntityManager::GetInstance();

	std::swap(m_uLevel,other.m_uLevel);
	std::swap(m_pParent,other.m_pParent);
	for (uint i=0; i<8; i++) 
	{
		std::swap(m_pChild[i],other.m_pChild[i]);
	}
}

Simplex::MyOctant::~MyOctant(void) { Release();}



//methods//////////////////////////////////////////////////////////////////////////////////////////
void Simplex::MyOctant::ConstructTree(uint a_nMaxLevel)
{
	m_uMaxLevel = a_nMaxLevel;
	m_uOctantCount = 1;
	m_EntityList.clear();
	KillBranches();
	m_lChild.clear();
	//only splits if entity list size is larger than ideal count and is not at maxlevel
	if (ContainsMoreThan(m_uIdealEntityCount))
	{
		Subdivide();//only leaves this statement when entire tree is created
	}
	AssignIDtoEntity();
	ConstructList();
}
void Simplex::MyOctant::Subdivide(void)
{	
	if (0!=m_uChildren||m_uLevel>=m_uMaxLevel) 
	{
		return;
	}
	//need 8 children octants
	m_uChildren = 8;
	float newSize= m_fSize/4.f;//amount it takes to get half way in any direction for child
	float doubleNewSize= newSize*2.f;//full size for child constructor
	
	//nested loop to condense the creation of children
	uint change=0;//allows index to rotate around properly
	for (int y = -1; y <= 1; y=y + 2) //manages center y
	{
		for (int a = -1; a <= 1; a=a + 2)//manages center x and z
		{
			vector3 newCenter = m_v3Center;//temp center v3 for manipulation in loop, to find center of children
			newCenter = vector3((float)a*newSize+newCenter.x, (float)y*newSize+newCenter.y, (float)a * newSize+newCenter.z);
			m_pChild[change] = new MyOctant(newCenter, doubleNewSize);//0,2,4,6

			newCenter = m_v3Center;//temp center v3 for manipulation in loop, to find center of children
			newCenter = vector3((float)-a * newSize+newCenter.x, (float)y*newSize+newCenter.y, (float)a * newSize+newCenter.z);
			m_pChild[change + 1] = new MyOctant(newCenter, doubleNewSize);//1,3,5,7
			change += 2;//increments by 2 each completion of a loop to allow for it to go around in --,+-,++,-+ loop
		}
	}
	for (uint a=0; a<8;a++) 
	{
		m_pChild[a]->m_pRoot = m_pRoot;
		m_pChild[a]->m_pParent = this;
		m_pChild[a]->m_uLevel = m_uLevel + 1;
		if (m_pChild[a]->ContainsMoreThan(m_uIdealEntityCount))
		{
			m_pChild[a]->Subdivide();//recursivly loops// only exits when all checked either fail level count or are less than ideal count
		}
	}
}


void Simplex::MyOctant::AssignIDtoEntity(void)
{
	//traverses the entire tree through recursion
	for (uint a = 0; a < m_uChildren; a++)
	{
		m_pChild[a]->AssignIDtoEntity();
	}
	if (m_uChildren==0) //has to stop when it is a leaf check
	{
		uint count = m_pEntityMngr->GetEntityCount();
		for (uint a=0; a<count;a++) 
		{
			//checks for collision between octet and entity rb
			if (IsColliding(a)) 
			{
				m_EntityList.push_back(a);//let octet know the id
				m_pEntityMngr->AddDimension(a,m_uID);//let entity know what dimension it belongs
			}
		}
	}
}
bool Simplex::MyOctant::IsColliding(uint a_uRBIndex)
{
	//first get the entity and its rigidbody
	MyRigidBody* currentEntityRB = m_pEntityMngr->GetEntity(a_uRBIndex)->GetRigidBody();
	//then check for collisons using simple collisions since all are cubes and not rotated
	vector3 max=currentEntityRB->GetMaxGlobal();
	vector3 min=currentEntityRB->GetMinGlobal();
	if (m_v3Min.x>max.x|| m_v3Min.z > max.z || m_v3Min.y > max.y|| m_v3Max.x < min.x || m_v3Max.z < min.z || m_v3Max.y <min.y)
	{
		return false;
	}
	return true;
}

bool Simplex::MyOctant::IsLeaf(void)
{
	if (m_uChildren == 0) {return true;}
	return false;
}

bool Simplex::MyOctant::ContainsMoreThan(uint a_nEntities)
{
	uint count = 0;
	for (uint a=0; a<m_pEntityMngr->GetEntityCount();a++)
	{
		if (IsColliding(a)) 
		{
			count++;
		}
		if (a_nEntities < count)//only splits if entity list size is larger than ideal count
		{
			return true;
		}
	}
	return false;
}

void Simplex::MyOctant::KillBranches(void)
{
	for (uint a = 0; a < m_uChildren; a++) 
	{
		m_pChild[a]->KillBranches();
		delete m_pChild[a];
		m_pChild[a] = nullptr;
	}
	m_uChildren = 0;
}


void Simplex::MyOctant::ConstructList(void)
{
	for (uint a=0; a<m_uChildren; a++) 
	{
		m_pChild[a]->ConstructList();
	}
	if(m_EntityList.size() >0) 
	{
		m_pRoot->m_lChild.push_back(this);
	}
}


void Simplex::MyOctant::ClearEntityList(void)
{
	//recursivly loop to all children then dump each of their entity lists
	for (uint a = 0; a < m_uChildren; a++)
	{
		m_lChild[a]->ClearEntityList();
	}
	m_EntityList.clear();
}


//Display////////////////////////////////////////
void Simplex::MyOctant::DisplayLeafs(vector3 a_v3Color)
{
	
	for (uint a = 0; a < m_lChild.size(); a++)
	{
		m_lChild[a]->DisplayLeafs(a_v3Color);
	}
	m_pMeshMngr->AddWireCubeToRenderList(glm::translate(IDENTITY_M4, m_v3Center) * glm::scale(vector3(m_fSize)), a_v3Color, RENDER_WIRE);
}

void Simplex::MyOctant::Display(uint a_nIndex, vector3 a_v3Color)
{
	if (m_uID==a_nIndex) 
	{
		m_pMeshMngr->AddWireCubeToRenderList(glm::translate(IDENTITY_M4, m_v3Center) * glm::scale(vector3(m_fSize)), a_v3Color, RENDER_WIRE);
		return;
	}
	for (uint a = 0; a < m_uChildren; a++)
	{
		m_lChild[a]->Display(a_nIndex);
	}
}

void Simplex::MyOctant::Display(vector3 a_v3Color)
{
	
	for (uint a = 0; a < m_uChildren; a++)
	{
		m_lChild[a]->Display(a_v3Color);
	}
	m_pMeshMngr->AddWireCubeToRenderList(glm::translate(IDENTITY_M4, m_v3Center) * glm::scale(vector3(m_fSize)), a_v3Color, RENDER_WIRE);
	
}

