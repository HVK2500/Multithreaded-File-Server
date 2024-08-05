// Demo_Server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Demo_Server.h"
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
		int receive = s.Receive(&buffer[i], buffer_size - i);
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
	vector<fstream*> fin;
	for (int i = 0; i < n; i++)
	{
		fstream* in = new fstream(files[i], ios::in | ios::binary);
		if (!in->is_open()) cout << "File " << files[i] << " errored" << endl;
		else  cout << "Successfully opened " << files[i] << endl;
		fin.push_back(in);
	}
	return fin;
}



void send_file_required(CSocket& s, vector<string>& normal, vector<string>& high, vector<string>& critical)
{
	// Send critical files's size information
	int n_critical = critical.size();
	cout << "Sending the number of critical files: " << n_critical << endl;
	vector<int> critical_size;
	s.Send((char*)&n_critical, sizeof(int));
	for (int i = 0; i < n_critical; i++)
	{
		int size = std::filesystem::file_size(critical[i]);
		critical_size.push_back(size);

		cout << "Sending critical " << critical[i] << " size: " << size << endl;
		s.Send((char*)&size, sizeof(int));
	}

	// Send high files's size information
	int n_high = high.size();
	cout << "Sending the number of high files: " << n_high << endl;
	vector<int> high_size;
	s.Send((char*)&n_high, sizeof(int));
	for (int i = 0; i < n_high; i++)
	{
		int size = std::filesystem::file_size(high[i]);
		high_size.push_back(size);

		cout << "Sending high " << high[i] << " size: " << size << endl;
		s.Send((char*)&size, sizeof(int));
	}

	// Send normal files's size information
	int n_normal = normal.size();
	cout << "Sending the number of normal files: " << n_normal << endl;
	vector<int> normal_size;
	s.Send((char*)&n_normal, sizeof(int));
	for (int i = 0; i < n_normal; i++)
	{
		int size = std::filesystem::file_size(normal[i]);
		normal_size.push_back(size);

		cout << "Sending normal " << normal[i] << " size: " << size << endl;
		s.Send((char*)&size, sizeof(int));
	}

	// Create file stream object for each critical file

	vector<fstream*> fin_critical = create_file_object(critical);
	vector<fstream*> fin_high = create_file_object(high);
	vector<fstream*> fin_normal = create_file_object(normal);

	// Create modes buffer
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
				fin_critical[i]->read(critical_buffer, ssize);
				send_buffer(s, critical_buffer, ssize);
				cout << "Send " << ssize << " byte from " << critical[i] << endl;
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
				fin_high[i]->read(high_buffer, ssize);
				send_buffer(s, high_buffer, ssize);
				cout << "Send " << ssize << " byte from " << high[i] << endl;
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
				fin_normal[i]->read(normal_buffer, ssize);
				send_buffer(s, normal_buffer, ssize);
				cout << "Send " << ssize << " byte from " << normal[i] << endl;
				normal_size[i] -= ssize;
			}
		}
	}

	cout << "Finish sending files...." << endl;
	// Delete buffer
	delete[] critical_buffer;
	delete[] high_buffer;
	delete[] normal_buffer;

	// Delete fstream object
	for (int i = 0; i < n_critical; i++)
	{
		fin_critical[i]->close();
		delete fin_critical[i];
	}
	for (int i = 0; i < n_high; i++)
	{
		fin_high[i]->close();
		delete fin_high[i];
	}
	for (int i = 0; i < n_normal; i++)
	{
		fin_normal[i]->close();
		delete fin_normal[i];
	}
}








DWORD WINAPI function_cal(LPVOID arg)
{
	SOCKET* hConnected = (SOCKET*) arg;
	CSocket s;
	// Convert to CSocket
	s.Attach(*hConnected);
	cout << "Connected to client" << endl;
	// Read client's input file
	int input_size;
	s.Receive((char*)&input_size, sizeof(int));
	string input_name = "input.txt";
	receive_file(s, input_name, input_size);
	cout << "Received input" << endl;

	// create list of required file
	vector<string> critical;
	vector<string> high;
	vector<string> normal;
	read_input(input_name, normal, high, critical);

	// Send required files
	send_file_required(s, normal, high, critical);


	


	delete hConnected;
	return 0;
	//return 0;
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
			CSocket server, s;
			DWORD threadID;
			HANDLE threadStatus;

			server.Create(4567);
			do {
				printf("Listening to clients....\n");
				server.Listen();
				server.Accept(s);
				//Khoi tao con tro Socket
				SOCKET* hConnected = new SOCKET();
				//Chuyển đỏi CSocket thanh Socket
				*hConnected	= s.Detach();
				//Khoi tao thread tuong ung voi moi client Connect vao server.
				//Nhu vay moi client se doc lap nhau, khong phai cho doi tung client xu ly rieng
				threadStatus = CreateThread(NULL, 0, function_cal, hConnected, 0, &threadID);
			}while(1);
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


