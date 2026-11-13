import i18n from "i18next";
import { initReactI18next } from "react-i18next";

import bn from "./locales/bn.json";
import en from "./locales/en.json";

const saved = typeof localStorage === "undefined" ? null : localStorage.getItem("fastrobox_lang");

void i18n.use(initReactI18next).init({
  resources: {
    en: { translation: en },
    bn: { translation: bn },
  },
  lng: saved === "bn" ? "bn" : "en",
  fallbackLng: "en",
  initImmediate: false, // resources are bundled: init synchronously
  interpolation: { escapeValue: false },
  react: { useSuspense: false },
});

export default i18n;
