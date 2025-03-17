Copyright Szabo Cristina-Andreea 2024-2025

# BitTorrent Simulator

This project implements a BitTorrent protocol simulator using MPI (Message Passing Interface). The simulator models the interaction between clients and a tracker in a peer-to-peer file-sharing network, handling file distribution, download, and upload processes using multiple threads.

## How It Works

1. **Client Initialization**:
   - Each client reads the files it owns and their corresponding data.
   - This information is sent to the tracker to build a list of all available files and their associated metadata.

2. **Tracker Processing**:
   - The tracker receives file data from clients sequentially.
   - It organizes and stores this data, creating a swarm for each file.
   - After processing all clients' data, it sends a confirmation signal to all clients.

3. **Download and Upload Threads**:
   - Clients start download and upload threads.
   - Each client requests the tracker for information about the swarm of the files it needs.
   - The tracker responds with the swarm and chunk details (including hash values for validation).

4. **File Chunk Requests**:
   - Clients request missing chunks from other peers in the swarm using a round-robin approach.
   - If a peer possesses the requested chunk, it responds positively; otherwise, it declines.
   - Clients periodically refresh the swarm information from the tracker to ensure they have the latest data.

5. **File Assembly and Validation**:
   - Once a client has collected all chunks of a file, it marks the file as complete.
   - The completed file is saved to an output file with a specific naming convention.

6. **Termination**:
   - Clients notify the tracker when they have finished downloading all required files.
   - Once the tracker receives a 'DONE' message from all clients, it signals them to terminate their upload threads and concludes its own execution.

## Key Features
- **Consistent Hashing**: Efficient distribution of file chunks across multiple servers.
- **Multi-threading**: Separate threads handle downloading and uploading for concurrency.
- **Round-Robin Strategy**: Optimized distribution of chunk requests among peers.
- **Dynamic Swarm Updates**: Ensures accurate peer information during the file-sharing process.
- **File Validation**: Chunk hash validation ensures data integrity.

