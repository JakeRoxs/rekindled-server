/*
 * Rekindled Server WebUI - API client module
 * Centralized fetch wrapper with auth headers, JSON handling, and error reporting.
 */

// Auth token storage key
const AUTH_TOKEN_KEY = "auth-token";

// Stores authentication token in localStorage.
function storeAuthToken(token) {
    if (token) {
        localStorage.setItem(AUTH_TOKEN_KEY, token);
    } else {
        localStorage.removeItem(AUTH_TOKEN_KEY);
    }
}

// Loads authentication token from localStorage.
function getAuthToken() {
    return localStorage.getItem(AUTH_TOKEN_KEY) || "";
}

// Shows a non-blocking error toast to the user.
function showApiError(message) {
    console.error("[WebUI] API error:", message);

    // Create or reuse toast container
    let toast = document.querySelector("#api-error-toast");
    if (!toast) {
        toast = document.createElement("div");
        toast.id = "api-error-toast";
        toast.className = "api-error-toast";
        toast.style.position = "fixed";
        toast.style.bottom = "20px";
        toast.style.right = "20px";
        toast.style.backgroundColor = "#d32f2f";
        toast.style.color = "white";
        toast.style.padding = "12px 16px";
        toast.style.borderRadius = "4px";
        toast.style.boxShadow = "0 2px 8px rgba(0,0,0,0.3)";
        toast.style.zIndex = "10000";
        toast.style.maxWidth = "400px";
        toast.style.transition = "opacity 0.3s";
        document.body.appendChild(toast);
    }

    toast.textContent = message;
    toast.style.opacity = "1";

    // Auto-hide after 5 seconds
    clearTimeout(toast._hideTimer);
    toast._hideTimer = setTimeout(() => {
        toast.style.opacity = "0";
    }, 5000);
}

// Unified API request function.
// Automatically adds auth token, sets correct Content-Type, handles JSON parsing,
// and reports errors to the user.
//
// Parameters:
//   endpoint - URL path (e.g. "/players")
//   method - HTTP method (default "get")
//   body - Object to serialize as JSON (optional)
//
// Returns: Promise resolving to parsed JSON response.
// On 401, calls reauthenticate(). On other errors, shows error toast.
function apiRequest(endpoint, method, body) {
    method = (method || "get").toLowerCase();

    let headers = {
        "Auth-Token": getAuthToken(),
    };

    let options = {
        method: method,
        headers: headers,
    };

    if (body !== undefined) {
        headers["Content-Type"] = "application/json";
        options.body = JSON.stringify(body);
    }

    return fetch(endpoint, options).then((response) => {
        if (response.status === 401) {
            if (typeof reauthenticate === "function") {
                reauthenticate();
            }
            throw new Error("Authentication required");
        }

        if (!response.ok) {
            return response.text().then((text) => {
                let msg = `Request failed (${response.status})`;
                try {
                    let err = JSON.parse(text);
                    if (err.message) msg = err.message;
                } catch (e) {
                    if (text) msg = text;
                }
                showApiError(msg);
                throw new Error(msg);
            });
        }

        return response.json();
    }).catch((error) => {
        // If fetch itself failed (network error), show error
        if (error.message !== "Authentication required") {
            showApiError(error.message);
        }
        throw error;
    });
}

// Convenience wrapper for GET requests
function apiGet(endpoint) {
    return apiRequest(endpoint, "get");
}

// Convenience wrapper for POST requests
function apiPost(endpoint, body) {
    return apiRequest(endpoint, "post", body);
}

// Convenience wrapper for DELETE requests
function apiDelete(endpoint, body) {
    return apiRequest(endpoint, "delete", body);
}
