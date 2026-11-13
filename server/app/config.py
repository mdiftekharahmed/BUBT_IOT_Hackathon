from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    database_url: str = "postgresql+psycopg://fastrobox@127.0.0.1:54329/fastrobox"
    session_secret: str = "change-me"
    telegram_bot_token: str = ""
    cookie_secure: bool = False


settings = Settings()
