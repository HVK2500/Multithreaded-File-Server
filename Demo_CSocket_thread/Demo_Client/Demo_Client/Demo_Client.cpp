// Demo_Client.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Demo_Client.h"
#include "afxsock.h"
#include <string>
#include <fstream>
#include <filesystem>
#include <stdint.h>
#include <vector>


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// The one and only application object

CWinApp theApp;

using namespace std;

// Read client's input file
void read_input(string filename, vector<string>& normal, vector<string>& high, vector<string>& critical)
{
	fstream fin(filename, ios::in);
	if (!fin) cout << "File error";
	string name;
	while (getline(fin, name, ' '))
	{
		string priority;
		getline(fin, priority);
		if (priority == "CRITICAL")
		{
			critical.push_back(name);
		}
		else if (priority == "HIGH")
		{
			high.push_back(name);
		}
		else
		{
			normal.push_back(name);
		}
	}
}

// Send buffer to client
void send_buffer(CSocket& s, char* buffer, int buffer_size)
{
	int i = 0;
	while (i < buffer_size)
	{
		int sent = s.Send(&buffer[i], buffer_size - i);
		i += sent;
	}
}


// Receive buffer from client
void receive_buffer(CSocket& s, char* buffer, int buffer_size)
{
	int i = 0;
	while (i < buffer_size)
	{
		int receive = s.Receive((char*)&buffer[i], buffer_size - i);
		i += receive;
	}
}

void send_file(CSocket& s, string filename)
{
	fstream fin(filename, ios::in | ios::binary);
	if (!fin) cout << "Cannot open " << filename << endl;
	uintmax_t size = std::filesystem::file_size(filename); // Get file size
	cout << size << endl;
	const int BUFFER_SIZE = 1024;
	char buffer[BUFFER_SIZE];
	int i = size;
	while (i != 0)
	{
		int send_size = min(i, BUFFER_SIZE);
		fin.read((char*)&buffer, send_size);
		send_buffer(s, buffer, send_size);
		i -= send_size;
	}
	fin.close();
}

void receive_file(CSocket& s, string filename, int file_size)
{
	fstream fout(filename, ios::out | ios::binary);
	if (!fout) cout << "Cannot open " << filename << endl;
	const int BUFFER_SIZE = 1024;
	char buffer[BUFFER_SIZE];
	int i = file_size;
	while (i != 0)
	{
		int recv_size = min(i, BUFFER_SIZE);
		receive_buffer(s, buffer, recv_size);
		fout.write((char*)&buffer, recv_size);
		i -= recv_size;
	}
	fout.close();
}

// Create fstream object list
vector<fstream*> create_file_object(vector<string> files)
{
	int n = files.size();
	vector<fstream*> fout;
	for (int i = 0; i < n; i++)
	{
		fstream* out = new fstream(files[i], ios::out | ios::binary);
		if (!out->is_open()) cout << "File " << files[i] << " errored" << endl;
		else  cout << "Successfully opened " << files[i] << endl;
		fout.push_back(out);
	}
	return fout;
}

void receive_file_required(CSocket& s, vector<string>& normal, vector<string>& high, vector<string>& critical)
{
	// Receive critical files's information
	int n_critical;
	s.Receive((char*)&n_critical, sizeof(int));
	cout << "Receive the number of critical files: " << n_critical << endl;
	vector<int> critical_size;
	for (int i = 0; i < n_critical; i++)
	{
		int size;
		s.Receive((char*)&size, sizeof(int));
		cout << "Receive normal file: " << critical[i] << " size: " << size << endl;
		critical_size.push_back(size);
	}

	// Receive high files's information
	int n_high;
	s.Receive((char*)&n_high, sizeof(int));
	cout << "Receive the number of high files: " << n_high << endl;
	vector<int> high_size;
	for (int i = 0; i < n_high; i++)
	{
		int size;
		s.Receive((char*)&size, sizeof(int));
		cout << "Receive high file: " << high[i] << " size: " << size << endl;
		high_size.push_back(size);
	}

	// Receive normal file's information
	int n_normal;
	s.Receive((char*)&n_normal, sizeof(int));
	cout << "Receive the number of normal files: " << n_normal << endl;
	vector<int> normal_size;
	for (int i = 0; i < n_normal; i++)
	{
		int size;
		s.Receive((char*)&size, sizeof(int));
		cout << "Receive normal file: " << normal[i] << " size: " << size << endl;
		normal_size.push_back(size);
	}

	// Create file stream object for each file
	vector<fstream*> fout_critical = create_file_object(critical);
	vector<fstream*> fout_high = create_file_object(high);
	vector<fstream*> fout_normal = create_file_object(normal);

	int CHUNK_SIZE = 4096;
	const int CRITICAL_CHUNK_SIZE = CHUNK_SIZE * 10;
	char* critical_buffer = new char[CRITICAL_CHUNK_SIZE];

	const int HIGH_CHUNK_SIZE = CHUNK_SIZE * 4;
	char* high_buffer = new char[HIGH_CHUNK_SIZE];

	const int NORMAL_CHUNK_SIZE = CHUNK_SIZE;
	char* normal_buffer = new char[NORMAL_CHUNK_SIZE];

	bool isBreak = false;
	while (!isBreak)
	{
		isBreak = true;

		// Send critical

		for (int i = 0; i < n_critical; i++)
		{
			if (critical_size[i] > 0)
			{
				isBreak = false;
				int ssize = min(critical_size[i], CRITICAL_CHUNK_SIZE);
				receive_buffer(s, critical_buffer, ssize);
				fout_critical[i]->write(critical_buffer, ssize);
				cout << "Wrote " << ssize << " byte to " << critical[i] << endl;
				critical_size[i] -= ssize;
			}

		}

		// Send high

		for (int i = 0; i < n_high; i++)
		{
			if (high_size[i] > 0)
			{
				isBreak = false;
				int ssize = min(high_size[i], HIGH_CHUNK_SIZE);
				receive_buffer(s, high_buffer, ssize);
				fout_high[i]->write(high_buffer, ssize);
				cout << "Wrote " << ssize << " byte to " << high[i] << endl;
				high_size[i] -= ssize;
			}
		}

		// Send normal


		for (int i = 0; i < n_normal; i++)
		{
			if (normal_size[i] > 0)
			{
				isBreak = false;
				int ssize = min(normal_size[i], NORMAL_CHUNK_SIZE);
				receive_buffer(s, normal_buffer, ssize);
				fout_normal[i]->write(normal_buffer, ssize);
				cout << "Wrote " << ssize << " byte to " << normal[i] << endl;
				normal_size[i] -= ssize;
			}
		}
	}
	cout << "Finish receiving files..." << endl;
	// Delete buffer
	delete[] critical_buffer;
	delete[] high_buffer;
	delete[] normal_buffer;

	// Delete fstream object
	for (int i = 0; i < n_critical; i++)
	{
		fout_critical[i]->close();
		delete fout_critical[i];
	}
	for (int i = 0; i < n_high; i++)
	{
		fout_high[i]->close();
		delete fout_high[i];
	}
	for (int i = 0; i < n_normal; i++)
	{
		fout_normal[i]->close();
		delete fout_normal[i];
	}

}

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	int nRetCode = 0;

	HMODULE hModule = ::GetModuleHandle(NULL);

	if (hModule != NULL)
	{
		// initialize MFC and print and error on failure
		if (!AfxWinInit(hModule, NULL, ::GetCommandLine(), 0))
		{
			// TODO: change error code to suit your needs
			_tprintf(_T("Fatal Error: MFC initialization failed\n"));
			nRetCode = 1;
		}
		else
		{
			// TODO: code your application's behavior here.
			AfxSocketInit(NULL);
			CSocket client;
			
			client.Create();
			client.Connect(_T("127.0.0.1"), 4567);

			cout << "Connected to server...." << endl;

			//Send input file
			string input_name = "input.txt";
			int input_size = std::filesystem::file_size(input_name);
			client.Send((char*)&input_size, sizeof(int));
			cout << "Sent input size: " << input_size << endl;
			send_file(client, input_name);

			// create list of required file
			vector<string> critical;
			vector<string> high;
			vector<string> normal;
			read_input(input_name, normal, high, critical);

			// Receive required files
			receive_file_required(client, normal, high, critical);
		}
	}
	else
	{
		// TODO: change error code to suit your needs
		_tprintf(_T("Fatal Error: GetModuleHandle failed\n"));
		nRetCode = 1;
	}

	return nRetCode;
}
