from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    database_url: str = "postgresql+psycopg://fastrobox@127.0.0.1:54329/fastrobox"
    session_secret: str = "change-me"
    telegram_bot_token: str = ""
    telegram_chat_id: str = ""  # teacher's chat id, applied by seed.py
    cookie_secure: bool = False

    # judges' accounts (seed.py); empty means "unset"
    judge_admin_password: str = ""
    judge_teacher_password: str = ""
    judge_s1_password: str = ""
    judge_s2_password: str = ""
    judge_guardian_password: str = ""
    device_token: str = ""


settings = Settings()
