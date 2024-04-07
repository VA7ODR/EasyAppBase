#pragma once
#include <string>

#include "logger.hpp"

template<class T>
class LockedReference
{
	public:
		LockedReference(LoggingMutex & mtxIn, T & rRefIn, const std::string & sFile, const std::string & sFunc, int iLine) :
			mtx(mtxIn),
			m_lock(sFile, sFunc, iLine, mtx),
			pRef(&rRefIn)
		{

		}

		LockedReference(LockedReference && in) :
			mtx(in.mtx),
			m_lock(std::move(in.lock)),
			pRef(pRef)
		{

		}

		LockedReference &operator=(LockedReference && in)
		{
			mtx = in.mtx;
			m_lock = std::move(in.m_lock),
			pRef = in.pRef;
			return *this;
		}

		T* operator->()
		{
			return pRef;
		}

		T& operator*()
		{
			return *pRef;
		}

		operator T&()
		{
			return *pRef;
		}

		LoggingMutexLocker & lock()
		{
			return m_lock;
		}

	protected:
		void set(T & p)
		{
			pRef = &p;
		}

		LockedReference(LoggingMutex & mtxIn, const std::string & sFile, const std::string & sFunc, int iLine) :
			mtx(mtxIn),
			m_lock(sFile, sFunc, iLine, mtx)
		{

		}

		LoggingMutex& mtx;
		LoggingMutexLocker m_lock;
		T * pRef = nullptr;
};
