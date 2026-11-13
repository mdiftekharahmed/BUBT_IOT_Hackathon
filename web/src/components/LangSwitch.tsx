import { useTranslation } from "react-i18next";

/** EN / বাং toggle; the choice survives reloads via localStorage. */
export function LangSwitch() {
  const { i18n } = useTranslation();
  const change = (lng: string) => {
    void i18n.changeLanguage(lng);
    try {
      localStorage.setItem("fastrobox_lang", lng);
    } catch {
      /* private mode: language stays for this session only */
    }
  };
  return (
    <div className="flex gap-1" data-testid="lang-switch">
      <button
        type="button"
        onClick={() => change("en")}
        className="rounded border border-slate-300 px-2 py-0.5 text-xs"
      >
        {i18n.t("lang.en")}
      </button>
      <button
        type="button"
        onClick={() => change("bn")}
        className="rounded border border-slate-300 px-2 py-0.5 text-xs"
      >
        {i18n.t("lang.bn")}
      </button>
    </div>
  );
}
