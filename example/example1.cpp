#include <iostream>
#include <string.h>

using namespace std;

string encryption(string plain_text) {
	string cipher_text = plain_text;
	for (int i=0;i<cipher_text.length();i++) {
		cipher_text[i]++;
	}
	return cipher_text;
}

string decryption(string cipher_text) {
	string plain_text = cipher_text;
	for (int i=0;i<plain_text.length();i++) {
		plain_text[i]--;
	}
	return plain_text;
}

string get_msg() {
	string msg = "hello dixit how are you!";
	return msg;
}

void enable_trusted_code_execution() {
	return;
}

void disable_trusted_code_execution() {
	return;
}

int main () {
	enable_trusted_code_execution();
	for (int i=0;i<50000;i++) {
		string msg = get_msg();
		msg = encryption(msg);
		cout << "Cipher-text: " << msg << endl; 
		msg = decryption(msg);
		cout << "Plain-text: " << msg << endl;
	}
	disable_trusted_code_execution();
	return 0;
}


