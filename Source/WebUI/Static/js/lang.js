// lang.js
// By GoBlock2021

// Define default language
const defaultLang = "en-US";

// In-memory cache for loaded locale files
const localeCache = {};

function saveLanguage(lang) {
    localStorage.setItem("userLang", lang);
    loadLanguage(lang);
}

function loadLanguage(lang) {
    // Check cache first
    if (localeCache[lang]) {
        updatePageText(localeCache[lang]);
        return;
    }

    // Try to load the specified language JSON file
    fetch(`./locales/${lang}.json`)
        .then((response) => {
            if (!response.ok) {
                throw new Error("Invalid locales file!");
            }
            return response.json();
        })
        .then((data) => {
            // Cache the loaded locale
            localeCache[lang] = data;
            updatePageText(data);
        })
        .catch((error) => {
            console.error("There was a problem fetching the language file:", error);
            if (lang != defaultLang) {
                loadLanguage(defaultLang);
            }
        });
}

function updatePageText(data) {
    document.querySelectorAll("[data-i18n]").forEach((element) => {
        const key = element.getAttribute("data-i18n");
        element.textContent = data[key] || element.textContent;
    });

    window.dispatchEvent(new Event("lang-updated"));
}

function init() {
    let browserLang = navigator.language || navigator.userLanguage;
    let userLang = localStorage.getItem("userLang");
    if (userLang == null) {
        userLang = browserLang;
        saveLanguage(browserLang);
    }
    loadLanguage(userLang);
}

window.addEventListener("DOMContentLoaded", (event) => {
    init();
});

function generateJson() {
    // 这个函数可以用来生成一个JS模板用作本地化，如果需要，请直接在浏览器控制台执行
    // This function can be used to generate a JS template for localization.
    // Please execute it directly in the browser console if needed.

    const translations = {};
    document.querySelectorAll("[data-i18n]").forEach((element) => {
        const key = element.getAttribute("data-i18n");
        translations[key] = element.textContent;
    });
    console.log(JSON.stringify(translations, null, 2));
}
