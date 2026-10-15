// This file is part of the aMule Project.
// Copyright (c) 2003-2026 aMule Team ( https://amule-org.github.io )
// Licensed under the GNU General Public License, version 2 or later.

#include <muleunit/test.h>
#include <TerminationProcessAmuleApi.h>
#include <amule.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <wx/init.h>
#include <wx/mstream.h>

using namespace muleunit;

// This fixture owns only wxProcess and its streams, never an application.
void CamuleApp::OnAmuleApiTerminated(int, int)
{
	std::abort();
}

namespace
{
class CountingInput : public wxMemoryInputStream
{
public:
	CountingInput(const std::string &data, std::atomic<size_t> &read, std::atomic<unsigned> &destroyed)
	: wxMemoryInputStream(data.data(), data.size())
	, m_read(read)
	, m_destroyed(destroyed)
	{
	}

	~CountingInput() override { ++m_destroyed; }

protected:
	size_t OnSysRead(void *buffer, size_t size) override
	{
		const size_t count = wxMemoryInputStream::OnSysRead(buffer, size);
		m_read += count;
		return count;
	}

private:
	std::atomic<size_t> &m_read;
	std::atomic<unsigned> &m_destroyed;
};

void WaitForDrain(std::atomic<size_t> &out, std::atomic<size_t> &err, size_t size)
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while ((out < size || err < size) && std::chrono::steady_clock::now() < deadline) {
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	ASSERT_EQUALS(size, out.load());
	ASSERT_EQUALS(size, err.load());
}
} // namespace

DECLARE_SIMPLE(AmuleApiProcess)

TEST(AmuleApiProcess, FailedLaunchDestroysWaitingWorker)
{
	wxInitializer wx;
	ASSERT_TRUE(wx.IsOk());
	// No streams were attached and readiness was never published.
	CTerminationProcessAmuleApi process("API launch fixture", nullptr);
}

TEST(AmuleApiProcess, DrainBothStreamsAndReleaseBeforeWxTeardown)
{
	wxInitializer wx;
	ASSERT_TRUE(wx.IsOk());
	std::string data(256 * 1024, 'x');
	std::atomic<size_t> out{ 0 }, err{ 0 };
	std::atomic<unsigned> destroyed{ 0 };
	CTerminationProcessAmuleApi process("API launch fixture", nullptr);
	process.SetPipeStreams(
		new CountingInput(data, out, destroyed), nullptr, new CountingInput(data, err, destroyed));
	process.StartOutputDrain();
	WaitForDrain(out, err, data.size());
	process.ReleaseFromApp();
	ASSERT_EQUALS(2u, destroyed.load());
	ASSERT_TRUE(process.GetInputStream() == nullptr);
	ASSERT_TRUE(process.GetErrorStream() == nullptr);
	// Shutdown and a later termination/destructor may both release resources.
	process.ReleaseFromApp();
	ASSERT_EQUALS(2u, destroyed.load());
}

TEST(AmuleApiProcess, TerminationJoinsWorkerBeforeDestroyingStreams)
{
	wxInitializer wx;
	ASSERT_TRUE(wx.IsOk());
	std::string data(256 * 1024, 'x');
	std::atomic<size_t> out{ 0 }, err{ 0 };
	std::atomic<unsigned> destroyed{ 0 };
	auto *process = new CTerminationProcessAmuleApi("API launch fixture", nullptr);
	process->SetPipeStreams(
		new CountingInput(data, out, destroyed), nullptr, new CountingInput(data, err, destroyed));
	process->StartOutputDrain();
	// Terminate while the worker may still be reading. OnTerminate deletes the observer.
	process->OnTerminate(0, 0);
	ASSERT_EQUALS(2u, destroyed.load());
}
