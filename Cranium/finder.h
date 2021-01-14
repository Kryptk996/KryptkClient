﻿#pragma once
#include "pch.h"
#include "structs.h"

namespace Offsets
{
	inline static constexpr int32_t OffsetInternal = 0x4C;
	inline static constexpr int32_t Next = 0x20;
	inline static constexpr int32_t ClassPrivate = 0x10;
	inline static constexpr int32_t ChildProperties = 0x50;
	inline static constexpr int32_t SuperStruct = 0x40;
}

class GameObject;

class GameClass
{
public:
	GameObject* GetChildProperties()
	{
		return *reinterpret_cast<GameObject**>(this + Offsets::ChildProperties);
	}

	GameClass* GetSuperStruct()
	{
		return *reinterpret_cast<GameClass**>(this + Offsets::SuperStruct);
	}
};

class GameObject
{
public:
	GameClass* GetClass()
	{
		return *reinterpret_cast<GameClass**>(this + Offsets::ClassPrivate);
	}

	GameObject* GetNext()
	{
		return *reinterpret_cast<GameObject**>(this + Offsets::Next);
	}

	int32_t GetOffsetInternal()
	{
		return *reinterpret_cast<int32_t*>(this + Offsets::OffsetInternal);
	}
};

class ObjectFinder
{
	std::wstring m_currentObject;
	std::wstring m_objectType;
	GameObject* m_object;
	GameObject*& m_objectRef;

	static GameObject* InternalFindChildInObject(GameObject* inObject, std::wstring_view childName)
	{
		GameObject* propertyObject = nullptr;
		GameObject* next = inObject->GetNext();
		if (next == nullptr) return nullptr;

		const auto firstPropertyName = GetFirstName(reinterpret_cast<FField*>(inObject));

		if (firstPropertyName == childName)
		{
			return inObject;
		}

		while (next)
		{
			std::wstring nextName = GetFirstName(reinterpret_cast<FField*>(next));
			if (childName == nextName)
			{
				propertyObject = next;
				break;
			}
			else
			{
				next = next->GetNext();
			}
		}

		return propertyObject;
	}

	GameObject*& resolveValuePointer(GameObject* bastePtr, GameObject* prop) const
	{
		return *reinterpret_cast<GameObject**>(reinterpret_cast<uintptr_t>(m_object) + prop->GetOffsetInternal());
	}

	GameObject*& resolveArrayValuePointer(GameObject* bastePtr, GameObject* prop) const
	{
		return *reinterpret_cast<GameObject**>(*reinterpret_cast<GameObject**>(reinterpret_cast<uintptr_t>(m_object) + prop->GetOffsetInternal()));
	}

public:
	ObjectFinder(const std::wstring& currentObject, const std::wstring objectType, GameObject* object, GameObject*& objectRef) :
		m_currentObject(currentObject), m_objectType(objectType), m_object(object), m_objectRef(objectRef)
	{
	};

	UObject*& GetObj() const
	{
		return reinterpret_cast<UObject*&>(m_objectRef);
	}

	static ObjectFinder GetEngine(uintptr_t GEngine)
	{
		return ObjectFinder{L"GameEngine", L"None", reinterpret_cast<GameObject*>(GEngine), reinterpret_cast<GameObject*&>(GEngine)};
	}

	ObjectFinder Find(const std::wstring& objectToFind) const
	{
		return FindChildObject(objectToFind);
	}

	ObjectFinder FindChildObject(const std::wstring& objectToFind) const
	{
		GameClass* classPrivate = m_object->GetClass();
		GameObject* childProperties = classPrivate->GetChildProperties();
		GameObject* propertyFound = nullptr;

		if (childProperties)
		{
			propertyFound = InternalFindChildInObject(childProperties, objectToFind);
		}

		GameClass* superStruct = classPrivate->GetSuperStruct();

		while (superStruct && !propertyFound)
		{
			childProperties = superStruct->GetChildProperties();
			if (childProperties)
			{
				propertyFound = InternalFindChildInObject(childProperties, objectToFind);
				if (propertyFound) break;
			}
			superStruct = superStruct->GetSuperStruct();
		}

		GameObject* valuePtr = resolveValuePointer(m_object, propertyFound);

		const std::wstring type = GetFieldClassName(reinterpret_cast<FField*>(propertyFound));

		if (type == XOR(L"ArrayProperty"))
		{
			//this will return the first element in the array
			//TODO: recode this part
			valuePtr = *reinterpret_cast<GameObject**>(valuePtr);

			GameObject*& valuePtrRef = resolveArrayValuePointer(m_object, propertyFound);
			return ObjectFinder(objectToFind, type, valuePtr, valuePtrRef);
		}
		else
		{
			GameObject*& valuePtrRef = resolveValuePointer(m_object, propertyFound);
			return ObjectFinder(objectToFind, type, valuePtr, valuePtrRef);
		}
	}
};

inline UObject* FindActor(std::wstring name)
{
	ObjectFinder EngineFinder = ObjectFinder::GetEngine(uintptr_t(GEngine));
	ObjectFinder GameViewPortClientFinder = EngineFinder.Find(XOR(L"GameViewport"));
	ObjectFinder WorldFinder = GameViewPortClientFinder.Find(XOR(L"World"));
	ObjectFinder PersistentLevelFinder = WorldFinder.Find(XOR(L"PersistentLevel"));

	const DWORD AActors = 0x98;

	for (auto i = 0x00; i < READ_DWORD(PersistentLevelFinder.GetObj(), AActors + sizeof(void*)); i++)
	{
		const auto Actors = READ_POINTER(PersistentLevelFinder.GetObj(), AActors);

		const auto pActor = static_cast<UObject*>(READ_POINTER(Actors, i * sizeof(void*)));

		//printf("\n[Actor %i] %ls, Class : %ls\n", i, GetObjectFullName(pActor).c_str(), GetObjectFullName(pActor->Class).c_str());

		if (pActor != nullptr)
		{
			if (GetObjectFullName(pActor).starts_with(name))
			{
				return pActor;
			}
		}
	}

	return nullptr;
}