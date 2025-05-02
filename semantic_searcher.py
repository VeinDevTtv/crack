import sys
import os

# Suppress noisy warnings (optional)
# os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2' # For TensorFlow backend if used
# os.environ['TRANSFORMERS_VERBOSITY'] = 'error'

try:
    # Check if running within venv (simplistic check)
    if sys.prefix == sys.base_prefix:
        print("Warning: Not running in a Python virtual environment. Dependencies might conflict.", file=sys.stderr)
    
    from sentence_transformers import SentenceTransformer, util
    import torch
except ImportError as e:
    print(f"Error importing Python libraries: {e}", file=sys.stderr)
    print("Please ensure 'sentence-transformers' and 'torch' are installed in the active environment.", file=sys.stderr)
    print(f"Attempted to import from: {sys.executable}", file=sys.stderr)
    sys.exit(1)

MODEL = None
MODEL_NAME = None
DEVICE = 'cuda' if torch.cuda.is_available() else ('mps' if torch.backends.mps.is_available() else 'cpu')

def initialize_model(model_name: str = 'all-MiniLM-L6-v2'):
    """Loads the Sentence Transformer model into memory."""
    global MODEL, MODEL_NAME
    if MODEL is not None and MODEL_NAME == model_name:
        # print(f"Model '{model_name}' already initialized.", file=sys.stderr)
        return True

    print(f"Initializing Sentence Transformer model '{model_name}' on device '{DEVICE}'...", file=sys.stderr)
    try:
        MODEL = SentenceTransformer(model_name, device=DEVICE)
        MODEL_NAME = model_name
        print(f"Model '{model_name}' loaded successfully.", file=sys.stderr)
        return True
    except Exception as e:
        print(f"Error loading model '{model_name}': {e}", file=sys.stderr)
        MODEL = None
        MODEL_NAME = None
        return False

def get_embedding(text: str):
    """Generates an embedding for a single string."""
    if MODEL is None:
        print("Error: Model not initialized.", file=sys.stderr)
        return None
    try:
        # normalize_embeddings=True is often recommended for cosine similarity
        embedding = MODEL.encode(text, convert_to_tensor=True, device=DEVICE, normalize_embeddings=True)
        return embedding
    except Exception as e:
        print(f"Error generating embedding: {e}", file=sys.stderr)
        return None

def find_similar_chunks(query: str, chunks: list[str], top_n: int = 10):
    """Finds chunks most similar to the query."""
    if MODEL is None:
        print("Error: Model not initialized.", file=sys.stderr)
        return None
    if not chunks:
        return []

    try:
        print(f"Encoding query and {len(chunks)} chunks...", file=sys.stderr)
        query_embedding = MODEL.encode(query, convert_to_tensor=True, device=DEVICE, normalize_embeddings=True)
        # Important: show_progress_bar can cause issues when called from C++
        chunk_embeddings = MODEL.encode(chunks, convert_to_tensor=True, device=DEVICE, normalize_embeddings=True, batch_size=128)
        print("Encoding complete.", file=sys.stderr)

        # Compute cosine similarity
        # util.cos_sim returns a tensor of shape (len(query_embedding), len(chunk_embeddings))
        cosine_scores = util.cos_sim(query_embedding, chunk_embeddings)[0] # We have one query

        # Get top N matches
        # top_k returns (values, indices)
        top_results = torch.topk(cosine_scores, k=min(top_n, len(chunks)))

        results = []
        for score, idx in zip(top_results[0], top_results[1]):
            results.append({"index": int(idx.item()), "score": float(score.item())})
        
        print(f"Found {len(results)} matches.", file=sys.stderr)
        return results

    except Exception as e:
        print(f"Error during similarity search: {e}", file=sys.stderr)
        return None

# Example usage (for testing Python script directly)
if __name__ == "__main__":
    print("Testing semantic_searcher.py...")
    model_ok = initialize_model()
    if not model_ok:
        sys.exit(1)

    test_query = "How does photosynthesis work?"
    test_chunks = [
        "Photosynthesis is the process used by plants, algae and cyanobacteria to convert light energy into chemical energy.",
        "The Krebs cycle is a series of chemical reactions used by all aerobic organisms.",
        "Light-dependent reactions capture the energy of light and use it to make the energy-storage molecules ATP and NADPH.",
        "Carbon fixation occurs during the light-independent reactions.",
        "Mitochondria are the powerhouses of the cell."
    ]
    
    print(f"\nQuery: {test_query}")
    similarities = find_similar_chunks(test_query, test_chunks)
    
    if similarities:
        print("\nTop matches:")
        for result in similarities:
            print(f"  Chunk {result['index']} (Score: {result['score']:.4f}): {test_chunks[result['index']]}")
    else:
         print("\nNo similarities found or error occurred.") 