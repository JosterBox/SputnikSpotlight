import logging
import os

from flask import Flask
from flask_ask import Ask, request, session, question, statement


app = Flask(__name__)
ask = Ask(app, "/")
logging.getLogger('flask_ask').setLevel(logging.DEBUG)
app.config['app_id'] = os.getenv('amzn1.ask.skill.1954edba-e8b0-45bd-99a5-cd9465eb5859'

@ask.launch
def launch():
    speech_text = ''
    return question(speech_text).reprompt(speech_text).simple_card('', speech_text)


@ask.intent('light')
def light():
    speech_text = 'Hello world'
    return statement(speech_text).simple_card('Change', speech_text)


@ask.intent('AMAZON.HelpIntent')
def help():
    speech_text = 'Sputnik spotlight is a remote controlled lamp. Can change position, rotate and dimmer.'
    return question(speech_text).reprompt(speech_text).simple_card('', speech_text)


@ask.session_ended
def session_ended():
    return "{}", 200


if __name__ == '__main__':
    if 'ASK_VERIFY_REQUESTS' in os.environ:
        verify = str(os.environ.get('ASK_VERIFY_REQUESTS', '')).lower()
        if verify == 'false':
            app.config['ASK_VERIFY_REQUESTS'] = False
app.run(debug=True)
