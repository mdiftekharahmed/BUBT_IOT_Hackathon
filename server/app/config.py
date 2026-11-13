from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    database_url: str = "postgresql+psycopg://fastrobox:fastrobox@postgres:5432/fastrobox"
    session_secret: str = "change-me"
    telegram_bot_token: str = ""


settings = Settings()
